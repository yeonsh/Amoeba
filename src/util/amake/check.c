/*	@(#)check.c	1.4	96/02/27 13:05:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "idf.h"
#include "type.h"
#include "symbol2str.h"
#include "Lpars.h"
#include "error.h"
#include "expr.h"
#include "scope.h"
#include "dbug.h"
#include "slist.h"
#include "builtin.h"
#include "objects.h"
#include "check.h"

#if __STDC__
#define PROTO(x) x
#else
#define PROTO(x) ()
#endif

PRIVATE struct idf *ID_if, *ID_then, *ID_else;

PRIVATE struct expr *DefaultForType PROTO(( int type ));
PRIVATE void HandleSpecialCase PROTO(( struct expr *call, struct expr *arrow ));

PUBLIC void
CheckCallID(idp)
struct idf **idp;
{
    if ((*idp)->id_kind == 0) {
	SemanticIdError(*idp, "is not declared");
	(*idp)->id_kind |= I_ERROR;
    } else if (((*idp)->id_kind & (I_CALL|I_ERROR)) == 0) {
	SemanticIdError(*idp, "cannot be called");
	(*idp)->id_kind |= I_ERROR;
    }
}

/*ARGSUSED*/
PUBLIC void
CheckArg(func, count, arrow, ret_type, any_type)
struct idf *func;
int count;
struct expr *arrow;
int *ret_type;
int *any_type;
{
    struct expr *arg = arrow->e_right;

    DBUG_ENTER("CheckArg");

    if (arg->e_number == RETURN) {
	if (*ret_type != 0) {
	    SemanticIdError(func, "%%return may be specified only once");
	} else {
	    *ret_type = T_LIST_OF | T_STRING;
	}
    }
    if ((arrow->e_left->e_param->par_type->tp_indicator & T_ANY) != 0) {
	if (*any_type == 0) {	/* first occurence matching ANY */
	    *any_type = arg->e_type;
	}
    }
    DBUG_VOID_RETURN;
}


PUBLIC struct expr *
ArgPlace(call, arg_idp)
struct expr *call;
struct idf *arg_idp;
/* where to put the argument 'arg_idp' in the current call 'call' */
{
    ITERATE(call->e_args, expr, arrow, {
	if (arrow->e_left->e_param->par_idp == arg_idp) {
	    return(arrow);
	}
    });
    if ((call->e_func->id_kind & I_ERROR) == 0) {
	FuncError(call, "parameter unknown");
    }
    AddParam(call, dummy_param());
    return(ItemP(TailOf(call->e_args), expr));
}

PRIVATE struct expr *
DefaultForType(type)
int type;
{
    int t = type & T_TYPE;

    if ((t & T_LIST_OF) != 0) {
	return(empty_list());
    } else if ((t & T_BOOLEAN) != 0) {
	return(TrueExpr);
    } else if ((t & T_STRING) != 0) {
	return(StringExpr(""));
    } else if ((t & T_OBJECT) != 0) {
	return(ObjectExpr(FooObject));
    } else {
	Warning("default for type `%s' is %%false", type_name(type));
	return(FalseExpr);
    }
}
	
PRIVATE void
HandleSpecialCase(call, arrow)
struct expr *call;
struct expr *arrow;
/* Currently, the only special case is the if function.
 * The `else' argument may be omitted, and gets a default according to
 * the type of the `then' part.
 */
{
    struct param *par;

    if (call->e_func == ID_if &&
	arrow->e_left->e_param->par_idp == ID_else) {
	struct expr *then_arrow = ArgPlace(call, ID_then);

	if (then_arrow->e_right != NULL) {
	    arrow->e_right = DefaultForType(then_arrow->e_right->e_type);
	    return;
	} else {
	    par = then_arrow->e_left->e_param;
	}
    } else {
	par = arrow->e_left->e_param;
    }
    FuncError(call, "parameter `%s' not assigned", par->par_idp->id_text);
    arrow->e_right = DefaultForType(par->par_type->tp_indicator);
}

PUBLIC void
CheckAllArgsAssigned(call, ret_type)
struct expr *call;
int *ret_type;
{
    ITERATE(call->e_args, expr, arrow, {
	register struct param *param = arrow->e_left->e_param;

	if (arrow->e_right == NULL) {		/* arg not assigned to */
	    if (param->par_default == NULL) {	/* no default available */
		if ((param->par_flags & PAR_COMPUTED) == 0) {
		    if ((call->e_func->id_flags & F_SPECIAL_FUNC) != 0) {
			HandleSpecialCase(call, arrow);
		    } else {
			FuncError(call, "parameter `%s' not assigned",
				  param->par_idp->id_text);
			arrow->e_right =
			    DefaultForType(param->par_type->tp_indicator);
		    }
		}
	    } else {
		/* tools that have RETURN as default return a list of strings
		 * instead of a boolean.
		 */
		if (*ret_type == 0 && param->par_default->e_number == RETURN) {
		    *ret_type = T_LIST_OF | T_STRING;
		}
	    }
	}
    });
}

PUBLIC void
check_init()
{
    ID_if = str2idf("if", 0);
    ID_then = str2idf("then", 0);
    ID_else = str2idf("else", 0);
}
