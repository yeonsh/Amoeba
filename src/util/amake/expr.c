/*	@(#)expr.c	1.3	94/04/07 14:50:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "alloc.h"
#include "dump.h"
#include "idf.h"
#include "Lpars.h"
#include "symbol2str.h"
#include "error.h"
#include "type.h"
#include "dbug.h"
#include "objects.h"
#include "slist.h"
#include "scope.h"
#include "tools.h"
#include "assignment.h"
#include "generic.h"
#include "main.h"
#include "expr.h"
#include "builtin.h"

#if __STDC__
#define PROTO(x) x
#else
#define PROTO(x) ()
#endif

PUBLIC struct expr *DiagExpr, *IgnoreExpr;
PUBLIC struct expr *EmptyListStringExpr, *EmptyListFileExpr;
PUBLIC struct expr *TrueExpr, *FalseExpr, *UnKnown;

PUBLIC short priority[NO_TOK] = { 0 };

PUBLIC char *free_expr = NULL;
/* expression free list */

#define EXPR_ALLOC	80	/* # nodes allocated at once */
#define EXPR_SIZE	sizeof(struct expr)

#ifdef DEBUG
PUBLIC int cnt_expr = 0;

#ifdef EXPR_DEBUG
PRIVATE struct slist *AllExprs = NULL;
#endif

#endif

PRIVATE struct slist *copy_list PROTO(( struct slist *list ));

PUBLIC struct expr *
new_expr(tok)
int tok;
/* creates new expression node of kind 'tok' */
{
    register struct expr *e;

#ifdef DEBUG
    (e = (struct expr *)std_alloc(&free_expr, EXPR_SIZE,
				  EXPR_ALLOC, &cnt_expr))->e_number = tok;
#ifdef EXPR_DEBUG
    Append(&AllExprs, e);
#endif
#else
    (e = (struct expr *)st_alloc(&free_expr, EXPR_SIZE,
				 EXPR_ALLOC))->e_number = tok;
#endif
    return(e);
}

#ifdef EXPR_DEBUG
PRIVATE void
Report(s)
char *s;
{
    Message("%s", s);
}

PrintExpres(e)
struct expr *e;
{
    if (IsList(e)) {
	int i = 0;

	Message("{");
	ITERATE(e->e_list, exr, arg, {
	    if (i++ > 10) {
		Message("..."); break;
	    } else {
		PrintExpres(arg);
		if (Next(cons) != NULL) {
		    Message(", ");
		}
	    }
	});
	Message("}");
    } else {
	PrExpr(e);
    }
}

PUBLIC void
ReportExprs()
{
    int i;

    if (Verbosity < 4) {
	return;
    }

    Write = Report;
    ITERATE(AllExprs, expr, e, {
	if (e->e_type != -1 && e->e_number != 0) {
	    Message("%6d:", i++);
	    PrintExpres(e);
	    Message("\n");
	    e->e_type = -1;
	}
    });
    Write = DefaultWriter;
}
#endif
		
PRIVATE struct slist *SharedValues = NULL;

PUBLIC void
MakeShared(list)
struct expr *list;
{
    assert(IsList(list));
    if (!IsShared(list)) {
	list->e_number = '}';
	Append(&SharedValues, list);
    }
}

PUBLIC struct expr *
empty_list()
{
    register struct expr *empty;

    (empty = new_expr('{'))->e_type = T_LIST_OF | T_SPECIAL;
    return(empty);
}

#define do_put_expr_node(node) st_free(node, &free_expr, EXPR_SIZE)

PUBLIC void
put_expr_node(node)
register struct expr *node;
{
    DBUG_EXECUTE("putexpr", {
	DBUG_WRITE(("put node (%d)", node->e_number)); DBUG_NL();
    });
    do_put_expr_node(node);
}

#define def_expr(Expr,Exnum,Tpnum) (Expr = new_expr(Exnum))->e_type = Tpnum

PUBLIC struct param *
dummy_param()
{
    static char *dummy_str = "_dummy_XXX";
    static int last_dummy = 0;
    struct idf *dummy;
    struct param *par;

    (void)sprintf(dummy_str + 7, "%d", last_dummy++);
    dummy = str2idf(dummy_str, 1);
    dummy->id_kind |= I_PARAM;
    par = dummy->id_param = new_param();
    par->par_idp = dummy;
    par->par_type = ErrorType;
    return(par);
}

PUBLIC void
expr_init()
{
    priority['{'] = -1; /* expression/cluster conflict */

    priority[OR] = 1;
    priority[AND] = 2;
    priority[NOT_EQ] = priority[EQ] = 3;
    priority['+'] = priority['\\'] = 4;		/* list operators */
    priority['&'] = 5;				/* concatenation */
    priority['?'] = priority['['] = 5;		/* attribute getting/setting*/
    priority['/'] = 6;				/* selection */
    priority[NOT] = 7;

    def_expr(DiagExpr, DIAG, T_SPECIAL|T_OBJECT);
    def_expr(IgnoreExpr, IGNORE, T_SPECIAL|T_OBJECT);
    def_expr(EmptyListStringExpr, '{', T_LIST_OF|T_STRING);
    def_expr(EmptyListFileExpr, '{', T_LIST_OF|T_OBJECT);
    def_expr(TrueExpr, TTRUE, T_BOOLEAN);
    def_expr(FalseExpr, FFALSE, T_BOOLEAN);
    def_expr(UnKnown, UNKNOWN, T_SPECIAL);

}

#ifdef ORIGINAL

#define DO_ADD(call, param) \
{       register struct expr *arg = new_expr(ARROW);    \
                                                        \
        arg->e_left = new_expr(FORMAL);                 \
        arg->e_left->e_param = param;                   \
        Append(&call->e_args, arg);                     \
}

#else

#define DO_ADD(call, param) \
{	register struct expr *arg = new_expr(ARROW);	\
							\
	if (param->par_expr == NULL) {			\
		param->par_expr = new_expr(FORMAL);	\
		param->par_expr->e_param = param;	\
	}						\
	arg->e_left = param->par_expr;			\
	Append(&call->e_args, arg);			\
}

#endif

PUBLIC void
AddParam(call, param)
struct expr *call;
struct param *param;
{
    DO_ADD(call, param);
}

PUBLIC struct expr *
MakeCallExpr(func)
struct idf *func;
/* return a template-expression for filling in the arguments of a call */
{
    struct expr *call;
    struct slist *params;

    call = new_expr('(');
    call->e_func = func;

    if ((func->id_kind & I_ERROR) != 0) {
	/* this error (function not defined) was reported already */
	call->e_type = T_ERROR; return(call);	
    }

    switch (func->id_kind & (I_CALL|I_GENERIC)) {
    case I_TOOL:	params = func->id_tool->tl_scope->sc_scope; break;
    case I_FUNCTION:	params = func->id_function->fun_scope->sc_scope; break;
    case I_GENERIC:	params = func->id_generic->gen_scope->sc_scope; break;
    default:		params = NULL;
			CaseError("CheckArguments: %d", func->id_kind); break;
    }

    ITERATE(params, param, par,	DO_ADD(call, par));

    return(call);
}

PUBLIC struct expr *
StringExpr(s)
char *s;
{
    struct expr *str;

    str = new_expr(STRING);
    str->e_type = T_STRING;
    str->e_string = Salloc(s, (unsigned)(strlen(s) + 1));
    return(str);
}

PUBLIC struct expr *
IntExpr(l)
long l;
{
    struct expr *num;

    num = new_expr(INTEGER);
    num->e_type = T_INTEGER;
    num->e_integer = l;
    return(num);
}

PUBLIC struct idf *
do_get_id(id_expr)
struct expr *id_expr;
{
    switch (id_expr->e_number) {
    case ID_STRING:
    case ID:		return(id_expr->e_idp);
    case STRING:	return(str2idf(id_expr->e_string, 1)); /* in symtab */
    default:		CaseError("get_id %d", id_expr->e_number);
			return(ErrorId());
    }
}

PUBLIC char *
do_get_string(str_expr) /* was: get_string */
struct expr *str_expr;
{
    switch (str_expr->e_number) {
    case ID_STRING:	return(str_expr->e_idp->id_text);
    case STRING:	return(str_expr->e_string);
    default:
			CaseError("do_get_string %d", str_expr->e_number);
			return("");
    }
}


#define do_put_expr_list(list)				\
    if (!IsEmpty(list)) {				\
	register struct cons *cons, *next;		\
							\
	for (cons = Head(list); cons != NULL; ) {	\
	    put_expr(ItemP(cons, expr));		\
	    next = Next(cons);				\
	    free_cons(cons);				\
	    cons = next;				\
	}						\
	free_slist(list);				\
    }


PUBLIC void
put_expr_list(list)
struct slist *list;
{
    do_put_expr_list(list);
}

PUBLIC void
put_expr(e)
register struct expr *e;
/* free the expressiontree with root 'e' recursively */
{
    assert(e != NULL);

    DBUG_EXECUTE("putexpr", {
	DBUG_WRITE(("put_expr: ")); DBUG_Expr(e);	DBUG_NL();
    });

    switch (e->e_number) { 
	/* TrueExpr, FalseExpr, UnKnown and OBJECTS are unique */
    case TTRUE:
    case FFALSE:
    case OBJECT:
    case UNKNOWN:
	/* The next 2 are not unique, but the current aproach may in fact
	 * save some memory (most expressions are attributes, that can
	 * never be freed). What we really need is a ref count machanism.
	 */
    case STRING:
    case INTEGER:
	break;
    case FORMAL:
	break;	/* FORMAL also not copied */
    case ID_STRING:
    case ID:
    case DIAG:
    case BUSY:
    case IGNORE:
    case RETURN:
    case REFID:
    case PARAM:
    case POINTER:
	do_put_expr_node(e);
	break;
    case '[': case '=': case '?':
    case '+': case '/': case '&': case '\\':
    case AND: case OR:
    case EQ: case NOT_EQ:
	put_expr(e->e_left);
	put_expr(e->e_right);
	DBUG_PRINT("putexpr", ("putnode: %s", symbol2str(e->e_number)));
	do_put_expr_node(e);
	break;
    case ARROW:
	put_expr(e->e_left);
	if (e->e_right != NULL) {
	    put_expr(e->e_right);
	}
	do_put_expr_node(e);
	break;
    case NOT:
	put_expr(e->e_left);
	do_put_expr_node(e);
	break;
    case '(':
	do_put_expr_list(e->e_args);
	do_put_expr_node(e);		/* the '(' node */
	break;
    case '{':
	do_put_expr_list(e->e_list);
	do_put_expr_node(e);		/* the '{' node */
	break;
    case '}':
	DBUG_EXECUTE("putexpr", {
	    DBUG_WRITE(("don't free shared list:"));
	    DBUG_Expr(e); DBUG_NL();
	});
	break;
    default:
	CaseError("put_expr(): %d", e->e_number);
    }
}

PRIVATE struct slist *
copy_list(list)
struct slist *list;
{
    struct slist *new = NULL;

    ITERATE(list, expr, elem, {
	Append(&new, CopyExpr(elem));
    });
    return(new);
}

/*VARARGS1*/
PUBLIC struct expr *
CopyExpr(e)
struct expr *e;
/* return a copy of expression e.
 * PARAM nodes are replaced by the value indicated in the corresponding param
 * structure.
 */
{
    struct expr *node;

    assert(e != NULL);

    switch (e->e_number) { 
	/* TrueExpr, FalseExpr, UnKnown and OBJECTS are unique */
    case OBJECT:
    case TTRUE:
    case FFALSE:
    case UNKNOWN:
	/* INTEGER and STRING nodes are not unique, but copying them
	 * so that they may put_expr()ed is a big pain
	 */
    case STRING:
    case INTEGER: 
	node = e;
	break;
    case FORMAL:
	/* Do we really have to copy this one? A FORMAL is only used
	 * during the argument / default checking fase. 
	 * So don't copy, but also: don't throw away!
	 */
	node = e;
	break;
    case PARAM:
	assert(e->e_param != NULL);
	if (e->e_param->par_value == NULL) {
	    DBUG_PRINT("expr", ("copy non-valued parameter"));
	    node = new_expr(PARAM);
	    node->e_param = e->e_param;
	} else {
	    node = CopyExpr(e->e_param->par_value);
	}
	break;
    case BUSY:
    case DIAG:
    case IGNORE:
    case RETURN:
    case ID_STRING:
    case ID:
    case REFID:
    case POINTER:
	node = new_expr(0);
	*node = *e;		/* copy whole structure */
	break;
    case '[': case '=': case '?':
    case '+': case '/': case '&': case '\\':
    case AND: case OR:
    case EQ: case NOT_EQ:
	node = new_expr(e->e_number);
	node->e_type = e->e_type;
	node->e_left = CopyExpr(e->e_left);
	node->e_right = CopyExpr(e->e_right);
	break;
    case ARROW:
	node = new_expr(e->e_number);
	node->e_left = CopyExpr(e->e_left);
	if (e->e_right != NULL) {
	    node->e_right = CopyExpr(e->e_right);
	}
	break;
    case NOT:
	node = new_expr(e->e_number);
	node->e_type = e->e_type;
	node->e_left = CopyExpr(e->e_left);
	break;
    case '(':
	node = new_expr('(');
	node->e_type = e->e_type;
	node->e_func = e->e_func;		/* functie identifier node */
	node->e_args = copy_list(e->e_args);
	break;
    case '{':
	node = new_expr('{');
	node->e_type = e->e_type;
	node->e_list = copy_list(e->e_list);
	break;
    case '}':
	DBUG_EXECUTE("copyexpr", {
	    DBUG_WRITE(("copy shared value:")); DBUG_Expr(e); DBUG_NL();
	});
	node = e;
	break;
    default:
	CaseError("CopyExpr(): %d", e->e_number);
	node = NULL;
    }
    return(node);
}

PUBLIC struct expr *
CopySharedExpr(e)
struct expr *e;
/* if a (possibly shared) expression is going to be heavily hacked upon,
 * call this function to avoid propagating the changes to other places
 * also using the shared expression.
 */
{
    if (IsShared(e)) {
	struct expr *new = empty_list();

	new->e_type = e->e_type;
	new->e_list = copy_list(e->e_list);
	return(new);
    } else {
	return(e);
    }
}

PUBLIC void
PrList(list)
struct slist *list;
{
    struct cons *cons;

    for (cons = Head(list); cons != NULL; cons = Next(cons)) {
	PrExpr(ItemP(cons, expr));
	if (Next(cons) != NULL) {
	    Write(", ");
	}
    }
}

PUBLIC void
PrExpr(e)
struct expr *e;
/* PrExpr schrijft 'e' met haakjes op debug output. */
{
    int n;

#ifdef DEBUG
    if (e == NULL) {
	Write("@");
	return;
    }
#endif

    n = e->e_number;
    switch (n) {
    case STRING:
	Write(QuotedId(e->e_string));
	break;
    case OBJECT:
	Write(ObjectName(e->e_object));
	break;
    case TTRUE: case FFALSE:
    case DIAG: case IGNORE: case RETURN: case UNKNOWN:
	Write(symbol2str(n));
	break;
    case '{':
    case '}':
	Write("{"); PrList(e->e_list); Write("}");
	break;
    case '[':
	PrExpr(e->e_left);
	Write("["); PrExpr(e->e_right); Write("]");
	break;
    case '(':
	Write(e->e_func->id_text); Write("("); PrList(e->e_args); Write(")");
	break;
    case FORMAL:
    case PARAM:
	Write(e->e_param->par_idp->id_text);
	break;
    case POINTER:
	Write("*pointer*");
	break;
    case ID_STRING:
	Write(QuotedId(e->e_idp->id_text));
	break;
    case ID:
	Write(e->e_idp->id_text);
	break;
    case REFID:
	Write("$"); Write(e->e_variable->var_idp->id_text);
	break;
    case INTEGER:
        {   char num[30];

	    (void)sprintf(num, "%ld", e->e_integer);
	    Write(num);
	}
	break;
    case '=': case ARROW: case '?':
    case '+': case '\\': case '/': case '&':
    case AND: case OR:
    case EQ: case NOT_EQ:
	Write("(");
	PrExpr(e->e_left);
	Write(" "); Write(symbol2str(n)); Write(" ");
	PrExpr(e->e_right);
	Write(")");
	break;
    case NOT:
	Write("(");
	Write(symbol2str(n)); Write("("); PrExpr(e->e_left); Write(")");
	Write(")");
	break;
    default:
#ifdef DEBUG
        {   char num[30];

	    (void)sprintf(num, "bad case(%ld)", e->e_number);
	    Write(num);
	}
#else
	CaseError("PrExpr(): %d", n);
#endif	
    }
}



