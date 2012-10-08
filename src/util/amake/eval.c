/*	@(#)eval.c	1.5	94/04/07 14:49:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "alloc.h"
#include "idf.h"
#include "type.h"
#include "expr.h"
#include "eval.h"
#include "error.h"
#include "dbug.h"
#include "Lpars.h"
#include "parser.h"
#include "scope.h"
#include "slist.h"
#include "builtin.h"
#include "execute.h"
#include "conversion.h"
#include "assignment.h"
#include "invoke.h"
#include "caching.h"
#include "statefile.h"
#include "derive.h"
#include "objects.h"

PRIVATE struct expr
    *equal_fn		( /* e */ ),
    *not_equal_fn	( /* e */ ),
    *join_fn		( /* e */ ),
    *difference_fn	( /* e */ ),
    *concat_fn		( /* e */ ),
    *select_fn		( /* e */ ),
    *get_fn		( /* e */ ),
    *and_fn		( /* e */ ),
    *or_fn		( /* e */ ),
    *not_fn		( /* e */ ),
    *attach_fn		( /* e */ ),
    *makelist_fn	( /* e */ );

/*
 * Expression evaluation:
 * evaluate arguments (reduce to constants)
 * and call appropriate function (inline or each on its own?)
 *
 * Cases to consider are:
 * (1) Constant reached: just return it
 * (2) Operator with 1 argument
 * (3) Operator with 2 arguments
 *     - NOTE: some (like AND) don't always evaluate their arguments first.
 * (4) Function/Tool call with all arguments to evaluate
 * (5) Builtin Function with only a subset of the arguments to evaluate
 *
 * Maybe the following approach is best: create a separate C-function for
 * each Amake-function. 
 */

#define NODE_NO_ARG	1	/* constant/variable/parameter reference */
#define NODE_LEFT_ARG	2	/* evaluate left argument (to begin with) */
#define NODE_TWO_ARG	3	/* evaluate left & right first */
#define NODE_FUNCTION	4	/* evaluate argument list first (nonspecial) */
#define NODE_NO_TOP	5	/* may not appear as top node during Eval */

PRIVATE short node_kind[NO_TOK] = { 0 };

PUBLIC void
eval_init()
{
    node_kind[NOT] =
    node_kind['['] =
    node_kind[AND] =
    node_kind[OR] =	NODE_LEFT_ARG;

    node_kind[','] =			/* taken care of by EvalArgs */
    node_kind[ARROW] = 			/* dito */
    node_kind['='] =			/* used in derivations */
    node_kind[ID] =	NODE_NO_TOP;	/* only in function calls? */

    node_kind['{'] =
    node_kind['}'] =
    node_kind['('] =	NODE_FUNCTION;

    /* constants: */
    node_kind[UNKNOWN] =
    node_kind[ID_STRING] =
    node_kind[REFID] =
    node_kind[PARAM] =
    node_kind[FORMAL] =
    node_kind[OBJECT] =
    node_kind[STRING] =
    node_kind[INTEGER] =
    node_kind[TTRUE] =
    node_kind[FFALSE] =

    /* specials: */
    node_kind[BUSY] =
    node_kind[DIAG] =
    node_kind[IGNORE] =
    node_kind[RETURN] =	NODE_NO_ARG;

    node_kind[EQ] =
    node_kind[NOT_EQ] =
    node_kind['+'] =
    node_kind['\\'] =
    node_kind['/'] =
    node_kind['?'] =
    node_kind['&'] =	NODE_TWO_ARG;
}

PRIVATE struct expr *
ToolInvokation(call)
struct expr *call;
{
    struct slist *inputs, *outputs;

    DBUG_PRINT("eval", ("tool instantiation of %s", call->e_func->id_text));
    DetermineIO(call, &inputs, &outputs);
    Invoke(call, inputs, outputs);
    DBUG_PRINT("eval", ("invocation succeeded in one turn"));

    /* The node *call has been replaced by the result */
    return(call);
}    

PUBLIC int ReqType;
PRIVATE struct idf *Id_exec;
PRIVATE int EvalDiag = TRUE;
/* Flag telling whether we should evaluate a DIAG node. This is to avoid
 * the creation of lots of new diagnostic files (and corresponding internal
 * objects) for exec() calls that have to wait.
 */

PRIVATE struct expr *
DoCall(call)
struct expr *call;
{
    register struct idf *func;
    struct expr *res;

    func = call->e_func;
    if (call->e_type & T_ERROR) {
	/* don't evaluate a call that is not entirely correct */
	FatalError("tried to evaluate bad call to `%s'", func->id_text);
    }
    if ((func->id_flags & F_SPECIAL_FUNC) == 0) {	/* SPECIAL = no eval */
	EvalDiag = func != Id_exec;
	if ((func->id_kind & I_TOOL) == 0) {
	    /* evaluate arguments */
	    ITERATE(call->e_args, expr, arrow, {
		if (arrow->e_right == NULL) {
		    arrow->e_right =
			CopyExpr(arrow->e_left->e_param->par_default);
		}
		arrow->e_right = Eval(arrow->e_right);
	    });
	}
	/* Tools also compute in- and outputs, further, defaults may refer
	 * to previous parameters. This will all be done in DetermineIO.
	 */
    }
    ReqType = call->e_type;		       /* dirty hack used by exec_fn */
    if ((func->id_kind & I_TOOL) != 0) {
	return(ToolInvokation(call));		  /* cleans up 'call' itself */
    } else {
	res = (*func->id_function->fun_code)(call->e_args);
    }
    put_expr(call);
    return(res);
}

PUBLIC struct expr *
EvalDiagAlso(e)
struct expr *e;
/* just like Eval, only DIAG's are expanded as well */
{
    EvalDiag = TRUE;
    return(Eval(e));
}

PUBLIC struct expr *
Eval(e)
struct expr *e;
{
    struct expr *res;

    assert(e != NULL);

#ifdef DEBUG
    if (e->e_number < 0 || e->e_number >= NO_TOK) {
	FatalError("bad e->e_number: %d", e->e_number);
    }
#endif

    switch (node_kind[e->e_number]) {
    case NODE_NO_ARG:
	switch (e->e_number) {
	    /* most common cases first */
	case STRING:
	case OBJECT:
	case ID_STRING:
	case INTEGER:
	    return(e);
	case REFID:	res = GetVarValue(e->e_variable->var_idp);
	    		break;
	case BUSY:	res = HandleBusy(e);
	    		break;
	case FORMAL:	CaseError("Eval: FORMAL not expected",0);
	    		/*NOTREACHED*/
	case PARAM:	CaseError("Eval: PARAM not expected",0);
	    		/*NOTREACHED*/
	case DIAG:	if (EvalDiag) {
	    		    return(ObjectExpr(NewDiagFile()));
			} else {
			    return(e);
			}
	case IGNORE:	return(ObjectExpr(IgnoreFile));
	case RETURN:	return(ObjectExpr(NewReturnFile()));
	default:
	    return(e);
	}
	put_expr(e);
	break;
    case NODE_LEFT_ARG:
	e->e_left = Eval(e->e_left);	/* update node if Eval returns*/
	switch (e->e_number) {
	case AND:	res = and_fn(e); break;
	case OR:	res = or_fn(e); break;
	case NOT:	res = not_fn(e); break;
	case '[':	res = attach_fn(e); break;
	default:	CaseError("bad NODE_LEFT_ARG (%d)", e->e_number);
	    		/*NOTREACHED*/
	}
	put_expr(e);
	break;
    case NODE_TWO_ARG:
	e->e_left = Eval(e->e_left);
	e->e_right = Eval(e->e_right);
	switch (e->e_number) {
	case EQ:	res = equal_fn(e); break;
	case '+':	return(join_fn(e)); /* avoids a lot new();free() */
	case '/':	res = select_fn(e); break;
	case NOT_EQ:	res = not_equal_fn(e); break;
	case '&':	res = concat_fn(e); break;
        case '?':	res = get_fn(e); break;
	case '\\':	res = difference_fn(e); break;
	default:	CaseError("bad NODE_TWO_ARG (%d)", e->e_number);
	    		/*NOTREACHED*/
	}
	put_expr(e);
	break;
    case NODE_FUNCTION:
	switch (e->e_number) {
	case '(': 	res = DoCall(e); break;
	case '{': 	res = makelist_fn(e); break;
	    		/* These function throw away their own garbage away */
	case '}':	res = e; break; /* shared list is already evaluated */
	default:  	CaseError("bad NODE_FUNCTION (%d)", e->e_number);
	    	  	res = NULL; break;
	}
	/* Some builtin functions don't evaluate all args at invocation time.
	 * DoCall has to take care of this.
	 */
	break;
    default:
	CaseError("Eval got illegal top node (%d)", e->e_number);
	/*NOTREACHED*/
    }
    return(res);
}

#define int2Bool(expr)	((expr) ? TrueExpr : FalseExpr)

PUBLIC int
IsEqual(left, right)
struct expr *left, **right; /* watch out: right is ptr to ptr! */
{
    if (((*right)->e_type & T_TYPE) != (left->e_type & T_TYPE)) {
	if (((*right)->e_type) != T_SPECIAL) { /* don't change %unknown */
	    *right = GetExprOfType(*right, left->e_type);
	} else {
	    return(FALSE);
	}
    }
    if ((left->e_type & T_LIST_OF) == 0) {
	switch (left->e_type) {
	case T_STRING:
	{   register char *sleft, *sright;

	    /* string comparisons being very common try to do as much
	     * as possible inline
	     */
	    if (left->e_number == ID_STRING) {
		sleft = left->e_idp->id_text;
	    } else {
		assert(left->e_number == STRING);
		sleft = left->e_string;
	    }
	    if ((*right)->e_number == ID_STRING) {
		sright = (*right)->e_idp->id_text;
	    } else {
		assert((*right)->e_number == STRING);
		sright = (*right)->e_string;
	    }

	    return(sleft == sright || strcmp(sleft, sright) == 0);
	}
	case T_OBJECT:
	    return(left->e_object == (*right)->e_object);
	case T_SPECIAL: /* UNKNOWN (for example?) */
	    return(left->e_number == (*right)->e_number);
	case T_INTEGER:
	    return(left->e_integer == (*right)->e_integer);
	case T_BOOLEAN:
	    return(left->e_number == (*right)->e_number);
	default:
	    CaseError("is_equal: %d", left->e_type);
	    return(FALSE);
	}
    } else {
	register struct cons *lcons, *rcons;

	for (lcons = Head(left->e_list), rcons = Head((*right)->e_list);
	     lcons != NULL && rcons != NULL;
	     lcons = Next(lcons), rcons = Next(rcons)) {
	    if (!is_equal(ItemP(lcons, expr), (struct expr **)&Item(rcons))) {
		return(FALSE);
	    }
	}
	return((lcons == NULL) && (rcons == NULL));
    }
}

PUBLIC int
is_member(elem, list)
struct expr *elem, *list;
{
    register struct cons *cons;

    for (cons = Head(list->e_list); cons != NULL; cons = Next(cons)) {
	if (is_equal(elem, (struct expr **)&Item(cons))) {
	    return(TRUE);
	}
    }
    return(FALSE);
}

PRIVATE struct expr *
equal_fn(e)
struct expr *e;
{
    DBUG_ENTER("equal_fn");
    DBUG_RETURN(int2Bool(is_equal(e->e_left, &e->e_right)));
}

PRIVATE struct expr *
not_equal_fn(e)
struct expr *e;
{
    DBUG_ENTER("not_equal_fn");
    DBUG_RETURN(int2Bool(!is_equal(e->e_left, &e->e_right)));
}

PRIVATE struct expr *
join_fn(e)
struct expr *e;
/* Expression e is thrown away. */
{
    register struct expr *left, *right;

    left = e->e_left = GetList(e->e_left);
    right = e->e_right = GetList(e->e_right);

    if (IsShared(left) || IsShared(right)) {
	struct expr *res;

	/* avoid creating a non shared list when appending an empty one */
	if (IsEmpty(left->e_list)) {
	    put_expr_node(e);
	    put_expr(left);
	    return(right);
	} else if IsEmpty(right->e_list) {
	    put_expr_node(e);
	    put_expr(right);
	    return(left);
	}

	res = empty_list();
	DBUG_EXECUTE("shared", {
	    DBUG_WRITE(("join shared lists:"));
	    DBUG_Expr(left); DBUG_WRITE((",")); DBUG_Expr(right); DBUG_NL();
	});

	ITERATE(left->e_list, expr, elem, {
	    Append(&res->e_list, CopyExpr(elem));
	});
	ITERATE(right->e_list, expr, elem, {
	    Append(&res->e_list, CopyExpr(elem));
	});
	put_expr(e);
			
	return(res);
    } else {
	/* Avoid copying if possible */

	left->e_list = DestructiveAppend(left->e_list, right->e_list);
	put_expr_node(e);		/* the '+' node */
	put_expr_node(right);		/* unused '{' node */
	return(left);
    }
}
    
PRIVATE struct expr *
difference_fn(e)
struct expr *e;
{
    register struct expr *res, *elem;
    register struct cons *cons;

    DBUG_ENTER("difference_fn");
    e->e_left = GetList(e->e_left);
    e->e_right = GetList(e->e_right);
    res = empty_list();

    /* A marking algorithm is O(n) instead of the O(n^2) alg. used here */
    for (cons = Head(e->e_left->e_list); cons != NULL; cons = Next(cons)) {
	elem = ItemP(cons, expr);
	if (!is_member(elem, e->e_right)) {
	    Append(&res->e_list, CopyExpr(elem));
	}
    }
    DBUG_RETURN(res);
}

PRIVATE struct expr *
concat_fn(e)
struct expr *e;
{
    struct expr *res;
    register char *str, *sleft, *sright;

    DBUG_ENTER("concat_fn");
    sleft = do_get_string(e->e_left = GetString(e->e_left));
    sright = do_get_string(e->e_right = GetString(e->e_right));

    str = Malloc((unsigned)(strlen(sleft) + strlen(sright) + 1));
    (void) strcpy(str, sleft);
    (void) strcat(str, sright);

    res = new_expr(STRING);
    res->e_string = str;
    res->e_type = T_STRING;
    DBUG_RETURN(res);
}

PRIVATE struct expr *
get_fn(e)
struct expr *e;
{
    register struct idf *idp;

    e->e_right = GetString(e->e_right);
    idp = get_id(e->e_right);
    DeclareAttribute(idp);	/* maybe a completely new one */

    return(DoGetAttrValues(&e->e_left, idp));
}

PRIVATE struct expr *
select_fn(e)
struct expr *e;
{
    struct expr *res;
    struct idf *comp;

    DBUG_ENTER("select_fn");

    e->e_left = GetObject(e->e_left);
    /* 
     * We could always do a GetString followed by a get_id, but that is slow
     * in the most likely case (foo/bar with bar allocated as ID_STRING).
     */
    if (e->e_right->e_number == ID_STRING) {
	comp = e->e_right->e_idp;
    } else {
	e->e_right = GetString(e->e_right);
	comp = get_id(e->e_right);
    }
    res = ObjectExpr(ObjectWithinParent(comp, e->e_left->e_object));
    DBUG_RETURN(res);
}

/* next functions: left is already evaluated */

PRIVATE struct expr *
and_fn(e)
struct expr *e;
{
    struct expr *val;

    DBUG_ENTER("and_fn");
    e->e_left = GetBoolean(e->e_left);
    if (IsTrue(e->e_left)) {
	e->e_right = GetBoolean(Eval(e->e_right));
	val = CopyExpr(e->e_right);
    } else {
	assert(IsFalse(e->e_left));	/* check that it is evaluated */
	val = FalseExpr;
    }
    DBUG_RETURN(val);
}

PRIVATE struct expr *
or_fn(e)
struct expr *e;
{
    struct expr *val;

    DBUG_ENTER("or_fn");
    e->e_left = GetBoolean(e->e_left);
    if (IsFalse(e->e_left)) {
	e->e_right = GetBoolean(Eval(e->e_right));
	val = CopyExpr(e->e_right);
    } else {
	assert(IsTrue(e->e_left));	/* check that it is evaluated */
	val = TrueExpr;
    }
    DBUG_RETURN(val);
}

PRIVATE struct expr *
not_fn(e)
struct expr *e;
{
    struct expr *res;

    DBUG_ENTER("not_fn");
    e->e_left = GetBoolean(e->e_left);
    if (IsTrue(e->e_left)) {
	res = FalseExpr;
    } else {
	assert(IsFalse(e->e_left));
	res = TrueExpr;
    }
    DBUG_RETURN(res);
}

/* attach : (t = { LIST(OBJ), OBJ }) x LIST(ATTR '=' VALUE) -> t
 * evaluate left argument
 * evaluate the attribute specifications of the form "attr-name => expr"
 * attach them to the object (possible inconsistency with attrs present!)
 * return the object itself
 */

PRIVATE struct expr *
do_attach(obj, attr_list)
struct object *obj;
struct expr *attr_list;
{
    /* If we are reading the cache, the attributes have to be added in
     * scope reserved for it.
     * A better solution would be creating a (real, but empty) scope for each
     * cache entry.
     */
    int scope_nr = ReadingCache ? CacheScopeNr : AttributeScopeNr;

    ITERATE(attr_list->e_list, expr, spec, {
	PutAttrValue(obj, get_id(spec->e_left),
		     CopyExpr(spec->e_right), scope_nr);
    });
}

PRIVATE struct expr *
attach_fn(e)
struct expr *e;
/* Attach attributes to the object(s) and return them. */
{
    DBUG_ENTER("attach_fn");

    /* evaluate the attribute values */
    ITERATE(e->e_right->e_list, expr, spec, {
	assert(spec->e_number == '=');
	spec->e_right = Eval(spec->e_right);
    });
    if ((e->e_left->e_type & T_LIST_OF) != 0) {
	e->e_left = GetListOfType(e->e_left, T_OBJECT);
	ITERATE(e->e_left->e_list, expr, obj_expr, {
	    do_attach(obj_expr->e_object, e->e_right);
	});
    } else {
	e->e_left = GetObject(e->e_left);
	do_attach(e->e_left->e_object, e->e_right);
    }
    DBUG_RETURN(CopyExpr(e->e_left));
    /* should be done more efficient, but it works */
}

PRIVATE struct expr *
makelist_fn(brace)
struct expr *brace;
/* Nonimplemented Idea: Avoid re-evaluating a list */
{
    register struct cons *cons, *next;
    DBUG_ENTER("make_list_fn");

    for (cons = Head(brace->e_list); cons != NULL; cons = next) {
	register struct expr *elem = ItemP(cons, expr);

	next = Next(cons);
	elem = Eval(elem);
	if (IsList(elem)) {
	    /* we don't like lists containing lists: splice it in */
	    if (IsShared(elem)) {
		struct slist *insert = NULL;

		DBUG_EXECUTE("shared", {
		    DBUG_WRITE(("splice shared expr:"));
		    DBUG_Expr(elem); DBUG_NL();
		});
		ITERATE(elem->e_list, expr, e, {
		    Append(&insert, CopyExpr(e));
		});
		Splice(&brace->e_list, cons, insert);
	    } else {
		Splice(&brace->e_list, cons, elem->e_list);
		put_expr_node(elem);
	    }
	} else {
	    Item(cons) = (generic)elem;
	}
	cons = next;
    }
    if (IsEmpty(brace->e_list)) {
	brace->e_type = T_LIST_OF | T_ANY;
    } else {
	brace->e_type = T_LIST_OF | ItemP(HeadOf(brace->e_list), expr)->e_type;
    }
    DBUG_RETURN(brace);
}

PUBLIC void
InitEval()
{
    Id_exec = str2idf("exec", 0);
}
