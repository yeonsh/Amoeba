/*	@(#)conversion.c	1.3	94/04/07 14:48:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <ctype.h>
#include <stdio.h>
#include "global.h"
#include "standlib.h"
#include "alloc.h"
#include "error.h"
#include "type.h"
#include "idf.h"
#include "expr.h"
#include "objects.h"
#include "builtin.h"
#include "Lpars.h"
#include "slist.h"
#include "symbol2str.h"
#include "dbug.h"
#include "dump.h"
#include "statefile.h"
#include "conversion.h"

PRIVATE struct expr *FalseString, *TrueString, *UnknownString;
PRIVATE struct idf *Id_FALSE, *Id_false, *Id_TRUE, *Id_true;

/*
 * Instead of complete typechecking, which gave to much problems (attributes
 * variables, single name objects (in current context) vs. strings,
 * empty lists, incomprehensible coercion code, etc.) we use auto conversion.
 * The routines in this module can be called with an evaluated expression as
 * argument. If the required conversion can't be made, and we're not reading
 * the Statefile, a error is generated.
 */

PRIVATE void
safe_message(s)
char *s;
{
    Message("%s", s);
}

PRIVATE void
ConversionError(s)
char *s;
{
    /* Statefile module has to give error message itself */
    if (!ReadingCache) {
	FatalError(s);
    }
}

PRIVATE void
ExprTypeError(expr, type)
struct expr *expr;
int type;
{
    char expected[20];

    /* Statefile module has to give error message itself */
    if (!ReadingCache) {
	DBUG_EXECUTE("shared", {
	    DBUG_WRITE(("error node %lx:", expr)); DBUG_NL();
	});
	Write = safe_message;
	Message("type error in `"); PrExpr(expr); Message("'\n");
	(void)strcpy(expected, type_name(type));
	FatalError("type %s expected instead of %s",
		   expected, type_name(expr->e_type));
    }
}

PUBLIC struct expr *
GetBoolean(expr)
struct expr *expr;
{
    /* allow conversion of "FALSE", "false", "TRUE" and "true" to their
     * boolean counterparts.
     */
    switch (expr->e_type) {
    case T_BOOLEAN:
	return(expr);
    case T_STRING:
    {
	struct idf *idp = get_id(expr);

	if (idp == Id_false || idp == Id_FALSE) {
	    return(FalseExpr);
	} else if (idp == Id_false || idp == Id_FALSE) {
	    return(TrueExpr);
	}
	/* else fallthrough */
    }
    default:
	ExprTypeError(expr, T_BOOLEAN);
	return(NULL);
    }
}

PUBLIC struct expr *
GetObject(expr)
struct expr *expr;
{
    switch (expr->e_type) {
    case T_OBJECT:
	return(expr);
    case T_STRING: {
	struct object *obj = SystemNameToObject(do_get_string(expr));
	/* was: AmakePathToObject */

	put_expr(expr);
	return(ObjectExpr(obj));
    }
    default:
	ExprTypeError(expr, T_OBJECT);
	return(NULL);
    }
}

PUBLIC struct expr *
GetString(expr)
struct expr *expr;
{
    switch (expr->e_type) {
    case T_STRING:
	return(expr);
    case T_BOOLEAN:
	return(CopyExpr(IsTrue(expr) ? TrueString : FalseString));
    case T_OBJECT: {
	struct expr *res = StringExpr(SystemName(expr->e_object));

	put_expr(expr);
	return(res);
    }
    case T_SPECIAL:
	if (expr->e_number == UNKNOWN) {
	    return(CopyExpr(UnknownString));
	} else {
	    struct expr *res = StringExpr(symbol2str(expr->e_number));

	    put_expr(expr);
	    return(res);
	}
    case T_INTEGER: {
	char num[20];

	(void)sprintf(num, "%ld", expr->e_integer);
	return(StringExpr(num));
    }
    default:
	ExprTypeError(expr, T_STRING);
	return(NULL);
    }
}

PUBLIC struct expr *
GetInteger(e)
struct expr *e;
{
    switch (e->e_type) {
    case T_INTEGER:
	return(e);
    case T_STRING: {
	char *s = do_get_string(e);
	long num;

	if (isdigit(*s)) {
	    num = atol(s);
	} else {
	    num = 0;
	    /* Not correct, but this has happened while dumping the statefile.
	     * When the cause (bad Statefile or plain bug?) for this has
	     * been found, we will change it back to ExprTypeError again.
	     */
	}
	return(IntExpr(num));
    }
    default:
	ExprTypeError(e, T_INTEGER);
	return(NULL);
    }
}
	    

PUBLIC struct expr *
GetAny(expr)
struct expr *expr;
/* called when comparing an empty list with something else, for example */
{
    return(expr);
}

PRIVATE expr_fun
TypeFun(type)
int type;
{
    /* delivers a pointer to the function handling 'type' */
    switch (type) {
    case T_STRING:	return(GetString);
    case T_OBJECT:	return(GetObject);
    case T_BOOLEAN:	return(GetBoolean);
    case T_INTEGER:     return(GetInteger);
    case T_ANY:		return(GetAny);
    default:		FatalError("TypeFun: bad type %s", type_name(type));
			/*NOTREACHED*/
    }
}

PUBLIC struct expr *
GetList(expr)
struct expr *expr;
{
    if (!IsList(expr)) {
	/* return a list with a single element */
	register struct expr *res;

	res = empty_list();
	Append(&res->e_list, expr);
	res->e_type = expr->e_type | T_LIST_OF;
	return(res);
    } else {
	return(expr);
    }
}

PUBLIC struct expr *
GetListOfType(expr, type)
struct expr *expr;
int type;
{
    register struct expr *res;
    register struct cons *cons;
    expr_fun fun;

    res = GetList(expr);
    res->e_type &= ~T_BASE;
    res->e_type |= type;

    if (type != T_SPECIAL) {
	fun = TypeFun(type);
	for (cons = Head(res->e_list); cons != NULL; cons = Next(cons)) {
	    Item(cons) = (generic) fun(ItemP(cons, expr));
	}
    }
    return(res);
}

PUBLIC struct expr *
GetExprOfType(expr, type)
struct expr *expr;
int type;
{
    if ((type & T_LIST_OF) != 0) {
	return(GetListOfType(expr, type & ~T_LIST_OF));
    } else {
	struct expr *atom;

	if (IsList(expr)) {
	    if (IsEmpty(expr->e_list)) {
		ConversionError("can't convert empty list to atom");
		return(NULL);
	    } else {
		struct cons *cons = Head(expr->e_list);

		if (Next(cons) != NULL) {
		    ConversionError("can't convert non singleton to atom");
		    return(NULL);
		}
		if (IsShared(expr)) {
		    atom = CopyExpr(ItemP(cons, expr));
		} else {
		    atom = ItemP(cons, expr);
		    free_cons(cons);
		    put_expr_node(expr); /* '{' node */
		}
	    }
	} else {
	    atom = expr;
	}
	if (type == T_SPECIAL) {
	    return(atom);
	} else {
	    return((TypeFun(type))(atom));
	}
    }
}

PUBLIC void
ConversionInit()
{
    UnknownString = StringExpr("unknown");
    FalseString = StringExpr("false");
    TrueString = StringExpr("true");

    Id_FALSE = str2idf("FALSE", 0);
    Id_false = str2idf("false", 0);
    Id_TRUE = str2idf("TRUE", 0);
    Id_true = str2idf("true", 0);
}
