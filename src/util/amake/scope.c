/*	@(#)scope.c	1.3	94/04/07 14:53:44 */
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
#include "type.h"
#include "Lpars.h"
#include "error.h"
#include "main.h"
#include "slist.h"
#include "cluster.h"
#include "dbug.h"
#include "assignment.h"
#include "tools.h"
#include "generic.h"
#include "expr.h"
#include "builtin.h"
#include "scope.h"

PRIVATE struct scope *GlobalScope;
PRIVATE struct slist *ScopeStack = NULL;
PRIVATE int NextScope = GLOBAL_SCOPE;	/* number of scope last created */

PUBLIC struct scope *CurrentScope = NULL;

PUBLIC void
NewScope()
{
    CurrentScope = new_scope();
    CurrentScope->sc_number = NextScope++;
}

PUBLIC void
DeclareIdent(idp, kind)
struct idf **idp;
int kind;
{
    struct param *param;
    struct idf *id;

    if (kind == I_PARAM) {
	if ((*idp)->id_param != NULL) { /* idf al bekend */
	    SemanticIdError(*idp, "is already declared as parameter");
	    *idp = ErrorId();
	}
	(*idp)->id_kind |= I_PARAM;
	(*idp)->id_param = param = new_param();
	param->par_idp = *idp;
	param->par_type = ErrorType;
	Append(&CurrentScope->sc_scope, param);
    } else {
	/* There may not exist 2 instances of the same kind.
	 * Furthermore, the tool, function and generic namespace are shared.
	 */
	if ((kind != I_VARIABLE && ((*idp)->id_kind & kind) != 0) ||
	    ((kind & I_SHARED) != 0 && ((*idp)->id_kind & I_SHARED) != 0)) {
	    SemanticIdError(*idp, "may not be redefined");
	    *idp = ErrorId();
	}
	id = *idp;

	switch (kind) {
	case I_CLUSTER:
	    (id->id_cluster = new_cluster())->cl_idp = id; break;
	case I_TOOL:
	    (id->id_tool = new_tool())->tl_idp = id; break;
	case I_FUNCTION:
	    (id->id_function = new_function())->fun_idp = id; break;
	case I_GENERIC:
	    (id->id_generic = new_Generic())->gen_idp = id; break;
	case I_VARIABLE:
	    if ((id->id_kind & I_VARIABLE) != 0) {
		if ((id->id_variable->var_flags & V_CMDLINE) != 0) {
		    Verbose(1, "assignment to `%s' ignored", id->id_text);
		    break;
		} else {
		    if ((id->id_flags & F_QUIET) == 0) {
			Verbose(3, "variable `%s' redefined", id->id_text);
		    }
		} 
	    } else {
		(id->id_variable = new_variable())->var_idp = id;
		id->id_variable->var_state = V_ALLOC;
	    }
	    break;
	default:
	    CaseError("DeclareIdent: %d", kind); break;
	}
	id->id_kind |= kind;
    }
}

PUBLIC void
ClearScope()
/* used to remove a scope just built with NewScope and DeclareIdent */
{
    ITERATE(CurrentScope->sc_scope, param, par, par->par_idp->id_param = NULL);
    CurrentScope = GlobalScope;
}

PUBLIC void
RemoveScope()
{
    assert(!IsEmpty(ScopeStack));
    Remove(&ScopeStack, TailOf(ScopeStack));
    assert(!IsEmpty(ScopeStack)); /* at least the GlobalScope must be left */

    /* restore previous scope */
    CurrentScope = ItemP(TailOf(ScopeStack), scope);
    ITERATE(CurrentScope->sc_scope, param, par, par->par_idp->id_param = par);
}

PUBLIC void
AddScope(scope)
struct scope *scope;
{
    ITERATE(CurrentScope->sc_scope, param, par, par->par_idp->id_param = par);
    CurrentScope = scope;
    Append(&ScopeStack, scope);
}

PUBLIC void
ResetScopes()
{
    /* remove all scopes except GlobalScope */
    while (ItemP(TailOf(ScopeStack), scope) != GlobalScope) {
	DBUG_PRINT("scope", ("scope %d was left",
			     ItemP(TailOf(ScopeStack), scope)->sc_scope));
	Remove(&ScopeStack, TailOf(ScopeStack));
    }
}

PUBLIC void
InitScope()
{
    NewScope();
    GlobalScope = CurrentScope;
    assert(GlobalScope->sc_number == GLOBAL_SCOPE);
    Append(&ScopeStack, GlobalScope);
}

/*ARGSUSED*/
PUBLIC void
PrintScope(scope)
struct scope *scope;
{
    DBUG_EXECUTE("scope", {
	DBUG_WRITE(("#### Parameters ####")); DBUG_NL();
	ITERATE(scope->sc_scope, param, p, {
	    DBUG_WRITE(("# ")); Write(QuotedId(p->par_idp->id_text));
	    DBUG_WRITE((":\t%s", type_name(p->par_type->tp_indicator)));
	    DBUG_NL();
	});
	DBUG_WRITE(("####################")); DBUG_NL();
    });
}

