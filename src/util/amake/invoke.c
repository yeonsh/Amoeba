/*	@(#)invoke.c	1.3	94/04/07 14:51:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "global.h"
#include "alloc.h"
#include "Lpars.h"
#include "idf.h"
#include "dbug.h"
#include "error.h"
#include "slist.h"
#include "eval.h"
#include "expr.h"
#include "objects.h"
#include "execute.h"
#include "type.h"
#include "conversion.h"
#include "tools.h"
#include "scope.h"
#include "caching.h"
#include "builtin.h"
#include "cluster.h"
#include "main.h"
#include "os.h"
#include "invoke.h"

PRIVATE int ClassInvocation;

/* On what should we suspend, when an object is target of another cluster?
 * The tool computing it may not have started yet.
 * On the other hand, if we wait for the cluster computing it
 * we may have to wait too long. For the moment it will do, however.
 * A more efficient solution would be replacing this suspension on
 * the cluster by one on the tool responsible for it, as soon as it
 * becomes known.
 */

PRIVATE void
SuspendCurrentOnList(jobs)
struct slist *jobs;
{
    ITERATE(jobs, job, j, SuspendCurrent(j));
}

PRIVATE int
SourceOfCurrentCluster(obj)
struct object *obj;
{
    if (CurrentCluster != NULL) {
	return(IsTrue(GetAttrInScope(obj, Id_is_source,
				      CurrentCluster->cl_scope->sc_number)));
    } else {
	return(FALSE);
    }
}

PUBLIC void
AllocInputs(inputs)
struct slist *inputs;
{
    register struct cons *cons;

    for (cons = Head(inputs); cons != NULL; cons = Next(cons)) {
	register struct object *obj = ItemP(cons, expr)->e_object;
	struct expr *val;

	DBUG_PRINT("in", ("input: `%s':", SystemName(obj)));
	switch (obj->ob_flags & (O_INPUT|O_OUTPUT)) {
	case 0:
	    /* object not used yet, but it may be a subtarget */
	    if (CurrentCluster != NULL &&
		(val = GetAttrValue(obj, Id_target)) != UnKnown) {
		struct idf *clus;

		assert(IsList(val) && !IsEmpty(val->e_list));
		clus = ItemP(HeadOf(val->e_list), expr)->e_idp;
	        if (clus == CurrentCluster->cl_idp) {
		    /* this happens when a program is created, who needs
		     * *itself* in order to be built.
		     */
		    DBUG_PRINT("in", ("target of THIS cluster %s",
				      clus->id_text));
		} else {
		    DBUG_PRINT("in", ("target of cluster %s", clus->id_text));
		    DBUG_PRINT("suspend", ("wait for subtarget `%s'",
					   SystemName(obj)));
		    SuspendCurrent(clus->id_cluster->cl_job);
		}
	    } else if ((val = GetAttrValue(obj, Id_output)) != UnKnown) {
		DBUG_PRINT("in", ("will produced in current cluster"));
		assert(val->e_number == POINTER);
		DBUG_PRINT("suspend", ("wait for tool producing `%s'",
				       SystemName(obj)));
		SuspendCurrent((struct job *)val->e_pointer);
		/* the job producing it */
	    } else if (!SourceOfCurrentCluster(obj) &&
		       GetAttrValue(obj, Id_alias) == UnKnown) {
		/* neither a source nor generated within this cluster;
		 * it probably is the tool itself, or some extra library.
		 */
		DBUG_PRINT("in", ("implicit input"));
		PutAttrValue(obj, Id_implicit, TrueExpr, GLOBAL_SCOPE);
	    }
	    break;
	case O_INPUT:
	    if (SourceOfCurrentCluster(obj)) {
		DBUG_PRINT("in", ("source of this cluster"));
	    } else if ((val = GetAttrValue(obj, Id_output)) != UnKnown) {
		/* see (**) below */
		DBUG_PRINT("in", ("will produced in current cluster"));
		assert(val->e_number == POINTER);
		DBUG_PRINT("suspend", ("wait for tool producing `%s'",
				       SystemName(obj)));
		SuspendCurrent((struct job *)val->e_pointer);
		/* the job producing it */
	    } else if ((val = GetAttrValue(obj, Id_alias)) != UnKnown) {
		/* The alias is set, so it has been generated.
		 * Only proceed if it conforms the currently installed link
		 */
		struct object *calias;

		if ((calias = obj->ob_calias) != NULL) {
		    if (val->e_object != calias) {
			DBUG_PRINT("in",("other version currently installed"));
			if (!IsEmpty(obj->ob_working)) {
			    DBUG_PRINT("suspend", ("directory entry for `%s'",
						   SystemName(obj)));
			    SuspendCurrentOnList(obj->ob_working);
			} else {
			    DBUG_PRINT("in", (", but not used"));
			}
		    } else {
			DBUG_PRINT("in", ("already installed"));
		    }
		} else {
		    DBUG_PRINT("in", ("not yet installed"));
		}
	    } else {
		DBUG_PRINT("in", ("implicit input"));
		PutAttrValue(obj, Id_implicit, TrueExpr, GLOBAL_SCOPE);
	    }
	    break;
	case O_OUTPUT:
	    DBUG_PRINT("in", ("currently used as output"));
	    DBUG_PRINT("suspend", ("directory entry `%s'", SystemName(obj)));
	    SuspendCurrentOnList(obj->ob_working);
	    break;
	default:
	    CaseError("AllocInputs: %d", obj->ob_flags);
	}
    }
}

/* (**)
 * Tricky circumstance, which actually happened in the build of the Amoeba
 * utilities: m4 has (among others) source look.c, look has source look.c.
 * The look.o from look.c already had been produced, so look was being loaded.
 * The look.o for m4 was not yet produced, and (because the check here wasn't
 * added) the wrong look.o was loaded with m4.
 */

PRIVATE void
AllocOutputs(outputs)
struct slist *outputs;
{
    ITERATE(outputs, expr, obj_expr, {
	register struct object *obj = obj_expr->e_object;

	if ((obj->ob_flags & (O_INPUT|O_OUTPUT)) != 0) {

	    DBUG_PRINT("suspend", ("`%s' also output of other tool",
				   SystemName(obj)));
	    SuspendCurrentOnList(obj->ob_working);
	    DBUG_PRINT("out", ("`%s' currently used", SystemName(obj)));
	} else {
	    DBUG_PRINT("out", ("`%s' not used yet", SystemName(obj)));
	}
    });
}

PUBLIC void
Taint(obj)
struct object *obj;
{
    /* Only generated objects or cluster targets are tainted, and they both
     * are unique, thus we can put the Id_failed attribute globally.
     */
    Verbose(4, "taint `%s'", SystemName(obj));
    PutAttrValue(obj, Id_failed, TrueExpr, GLOBAL_SCOPE);
}
    
AddUsage(obj, job)
struct object *obj;
struct job *job;
{
    DBUG_PRINT("use", ("use `%s'", SystemName(obj)));
    Append(&obj->ob_working, job);
}

PUBLIC int
MarkInputs(inputs, job)
struct slist *inputs;
struct job *job;
{
    int failed = 0;
    register struct cons *cons;
    int must_exist = job == NULL; /* hack */

    DBUG_PRINT("inv", ("MarkInput, scope is %d", AttributeScopeNr));

    for (cons = Head(inputs); cons != NULL; cons = Next(cons)) {
	register struct object *obj = ItemP(cons, expr)->e_object, *the_obj;
	register struct expr *val;

	if (job != NULL) { 
	    AddUsage(obj, job);
	}
	obj->ob_flags |= O_INPUT;

	DBUG_PRINT("mark", ("`%s' marked input", SystemName(obj)));
	DBUG_EXECUTE("objects", { PrintObject(obj); });

	if ((val = GetAttrValue(obj, Id_alias)) != UnKnown) {
	    assert(val->e_number == OBJECT);
	    the_obj = val->e_object;
	} else {
	    the_obj = obj;
	}
	if (IsTrue(GetAttrValue(the_obj, Id_failed))) {
	    Verbose(4, "file `%s' is tainted", SystemName(the_obj));
	    failed++; 
	} else {
	    if (obj == the_obj) {
		if (!F_no_exec && !FileExists(obj)) {
		    if (GetAttrValue(obj, Id_err_reported) == UnKnown) {
			Verbose(1, "file `%s' not found", SystemName(obj));
			PutAttrValue(obj, Id_err_reported,
				     TrueExpr, GLOBAL_SCOPE);
		    }
		    if (must_exist) {
			failed++;
		    }
		    /* For the moment suppose it is an object like 'cc',
		     * which is found by shell in $PATH.
		     * We let the tool itself find out if it is really fatal.
		     */
		}
	    }
	}
    }
    return(failed == 0);
}

PRIVATE void
InstallInputs(inv)
struct invocation *inv;
/* The tool has to be (re-)run. Make sure generated inputs (probably made
 * in a previous run!) are installed.
 */
{
    ITERATE(inv->inv_inputs, expr, obj_expr, {
	register struct expr *alias;

	if ((alias = GetAttrValue(obj_expr->e_object, Id_alias)) != UnKnown) {
	    InstallGeneratedInput(obj_expr->e_object, alias->e_object);
	}
    });
}

PRIVATE void
MarkOutputs(outputs, job)
struct slist *outputs;
struct job *job;
{
    ITERATE(outputs, expr, obj_expr, {
	register struct object *obj = obj_expr->e_object;

	AddUsage(obj, job);
	obj->ob_flags |= O_OUTPUT;
	obj->ob_flags &= ~O_INPUT; /* can't be cleared in FreeInputs() */
	DBUG_PRINT("mark", ("`%s' marked output", SystemName(obj)));
    });
}

PRIVATE void
FreeOutputs(inv, outputs)
struct invocation *inv;
struct slist *outputs;
/* The tool that had obj as one of its outputs has finished, returning status.
 * If IsFalse(status), the command failed and the objects have to be
 * marked as such, so that the suspended commands waiting for it don't
 * use it.
 */
{
    ITERATE(outputs, expr, obj_expr, {
	register struct object *obj = obj_expr->e_object;

	DBUG_PRINT("inv", ("freeing output `%s'", SystemName(obj)));
	assert((obj->ob_flags & O_OUTPUT) != 0);
	obj->ob_flags &= ~O_OUTPUT;

	assert(ItemP(HeadOf(obj->ob_working), job) == inv->inv_job);
	(void)RemoveHead(&obj->ob_working, job);
	assert(IsEmpty(obj->ob_working));

	PutAttrValue(obj, Id_output, UnKnown, inv->inv_scope_nr);
	/* jobs in this cluster using this output may use it now */
    });
}

PRIVATE void
FreeInputs(inv)
struct invocation *inv;
{
    /* We cannot clear the O_INPUT flag, because at the moment we don't
     * adminstrate how many processes are using it.
     * Add an attribute `num_input' to the objects?
     */
    ITERATE(inv->inv_inputs, expr, obj_expr, {
	register struct object *obj = obj_expr->e_object;

	assert((obj->ob_flags & O_INPUT) != 0);
	DBUG_PRINT("use", ("freeing `%s'", SystemName(obj)));
	/* update the ob_working field */

	if (!IsEmpty(obj->ob_working)) {
	    register struct cons *cns;

	    for (cns = Head(obj->ob_working); cns != NULL; cns = Next(cns)) {
		if (ItemP(cns, job) == inv->inv_job) {
		    Remove(&obj->ob_working, cns);
		    break;
		}
	    }
	    assert(cns != NULL); /* assert("job was removed") */
	} else {
	    DBUG_PRINT("use", ("computed input?"));
	}
    });
}

/* Invokation is done in the same way as derivations are.
 * So a command is *not* put in the ReadyQ, but rather started up immediately
 * and continued later on when it suspends.
 */

PRIVATE void
DetermineTemps(inv)
struct invocation *inv;
/* Initialise the inv_temps field of the inv structure, containing the list
 * of temporary objects used by the invocation.
 */
{
    if ((inv->inv_tool_id->id_tool->tl_flags & TL_USES_TEMPS) != 0) {
	ITERATE(inv->inv_call->e_args, expr, arrow, {
	    register int type = arrow->e_left->e_param->par_type->tp_indicator;

	    if ((type & T_TMP) != 0) {
		if ((type & T_LIST_OF) == 0) {
		    Append(&inv->inv_temps, arrow->e_right);
		} else {
		    AppendList(&inv->inv_temps, arrow->e_right->e_list);
		}
	    }
	});
    }
}

PUBLIC struct job *
InvokationJob(header, inputs, outputs, invp)
struct expr *header;
struct slist *inputs;
struct slist *outputs;
struct invocation **invp;
{
    struct invocation *inv;
    struct job *inv_job;

    DBUG_EXECUTE("inv", {
	DBUG_WRITE(("inputs of the tool:")); PrList(inputs); DBUG_NL();
	DBUG_WRITE(("outputs of the tool:")); PrList(outputs); DBUG_NL();
    });

    inv = *invp = new_invocation();
    inv->inv_state = INV_START;
    inv->inv_tool_id = header->e_func;
    inv->inv_call = header;
    inv->inv_action = NULL; /* filled in on moment of execution */
    inv->inv_inputs = inputs;
    inv->inv_outputs = outputs;
    inv->inv_scope_nr = CurrentScope->sc_number;

    DetermineTemps(inv);

    DBUG_EXECUTE("inv", {
	DBUG_WRITE(("inv: ")); DBUG_Expr(inv->inv_call); DBUG_NL();
    });

    inv_job = CreateJob(ClassInvocation, (generic)inv);
    inv->inv_job = inv_job;
    return(inv_job);
}

PUBLIC void
PutParams(call)
struct expr *call;
{
    ITERATE(call->e_args, expr, arrow, {
	arrow->e_left->e_param->par_value = arrow->e_right;
    });
}


PRIVATE void
ReallyDoInvoke(invp)
struct invocation **invp;
{
    struct invocation *inv = *invp;
    int err = FALSE;

    DBUG_ENTER("DoInvoke");
    AttributeScopeNr = inv->inv_scope_nr;
    switch (inv->inv_state) {
    case INV_START:
	AllocInputs(inv->inv_inputs);
	AllocOutputs(inv->inv_outputs);
	AllocOutputs(inv->inv_temps);
	Wait(); /* but maybe we don't have to */
	inv->inv_state = INV_CHECK;	/* fall through */
    case INV_CHECK:
	MarkOutputs(inv->inv_outputs, inv->inv_job);
	MarkOutputs(inv->inv_temps, inv->inv_job);
	if (!MarkInputs(inv->inv_inputs, inv->inv_job)) {
	    Verbose(1, "`%s' not invoked because of tainted input",
		    inv->inv_tool_id->id_text);
	    err = TRUE;
	    inv->inv_action = FalseExpr;
	    break;
	}
	SolveSequenceProblems(inv, inv->inv_scope_nr);
	inv->inv_state = INV_CACHE;
    case INV_CACHE:
	/* Now, check the cache for a previous invocation.
	 * Put the result in inv_action (normally inv_action is reduced
	 * using `tree rewriting' to the resulting expression)
	 */
	inv->inv_state = INV_PICKUP;
	/* LookInCache may block, in this case the result is picked up later*/
    case INV_TRIGGER:
	LookInCache(inv); /* sets trigger (true or false) and action */
	if (IsTrue(inv->inv_trigger)) {
	    inv->inv_state = INV_INVOKE;
	    /* install the inputs that are not yet available. */
	    InstallInputs(inv);
	} else {
	    break;
	}
    case INV_INVOKE:
	DBUG_EXECUTE("inv", {
	    DBUG_WRITE(("action before: ")); DBUG_Expr(inv->inv_action);
	    DBUG_NL();
	});
	inv->inv_action = Eval(inv->inv_action);
	DBUG_EXECUTE("inv", {
	    DBUG_WRITE(("action after:  ")); DBUG_Expr(inv->inv_action);
	    DBUG_NL();
	});
	inv->inv_state = INV_END;
    case INV_END:
	break;
    default:
	assert(inv->inv_state == INV_PICKUP);
    }

    DBUG_EXECUTE("inv", {
	DBUG_WRITE(("inv: ")); DBUG_Expr(inv->inv_call); DBUG_NL();
    });

    if (!err) {
	HandleOutputs(inv->inv_call, inv->inv_cache, &inv->inv_action,
		      inv->inv_scope_nr);
    } else {
	ITERATE(inv->inv_outputs, expr, obj_expr, {
	    /* let following tools know about failure */
	    Taint(obj_expr->e_object);
	});
    }

    FreeInputs(inv);
    FreeOutputs(inv, inv->inv_outputs);
    FreeOutputs(inv, inv->inv_temps);

    if (inv->inv_call->e_type == (T_LIST_OF | T_STRING)) {
	if (!err) {
	    /* get the contents of the return file, and remove it */
	    ITERATE(inv->inv_call->e_args, expr, arrow, {
		register struct expr *arg = arrow->e_right;
	    
		if (arg->e_number == OBJECT &&
		    (arg->e_object->ob_flags & O_RETURN) != 0) {
		    inv->inv_action = GetContents(arg->e_object, TRUE, FALSE);
		    break;
		}
	    });
	} else {
	    (inv->inv_action = empty_list())->e_type = T_LIST_OF|T_STRING;
	}
    }

    DBUG_EXECUTE("inv", {
	DBUG_WRITE(("inv: ")); DBUG_Expr(inv->inv_call); DBUG_NL();
	DBUG_WRITE(("invocation result: ")); DBUG_Expr(inv->inv_action);
	DBUG_NL();
    });

    /* Free the *arguments* of the invocation header, and replace
     * the header top node by a copy of the result top node.
     * This is used in invocations that were not started by the cluster
     * algorithm: re-evaluation sees the result instead of header.
     */
    put_expr_list(inv->inv_call->e_args);
    *(inv->inv_call) = *(inv->inv_action);

    DBUG_VOID_RETURN;
}

PUBLIC void
FutureOutput(objects, job, scope_nr)
struct slist *objects;
struct job *job;
int scope_nr;
{
    ITERATE(objects, expr, obj, {
	/* We have to mark the output right now, even if we cannot start
	 * the tool (e.g, because the inputs aren't all available yet).
	 * This is to prevent starting subsequent tools in this cluster that
	 * need obj as input.
	 */
	register struct expr *ptr = new_expr(POINTER);

	ptr->e_pointer = (generic)job;
	PutAttrValue(obj->e_object, Id_output, ptr, scope_nr);
    });
}

PUBLIC void
DetermineIO(header, inputs, outputs)
/* Determines input and output objects. Also installs the arguments as
 * formal parameters of the right kind.
 */
struct expr *header;
struct slist **inputs, **outputs;
{
    *inputs = *outputs = NULL;
    AddScope(header->e_func->id_tool->tl_scope);
    ITERATE(header->e_args, expr, arrow, {
	struct param *param = arrow->e_left->e_param;
	int type = param->par_type->tp_indicator;

	DBUG_PRINT("bug", ("handling parameter %s", param->par_idp->id_text));
	if (arrow->e_type == T_SPECIAL) continue; /* already done */

	if (arrow->e_right == NULL) {
	    EvalDefault(header, arrow);
	} else {
	    arrow->e_right = Eval(arrow->e_right);
	    arrow->e_right = GetExprOfType(arrow->e_right, type & T_TYPE);
	}
	if ((type & T_OUT) != 0) {
	    if ((type & T_LIST_OF) == 0) {
		Append(outputs, arrow->e_right);
	    } else {
		AppendList(outputs, arrow->e_right->e_list);
	    }
	} else if ((type & T_IN) != 0) {
	    if ((type & T_LIST_OF) == 0) {
		Append(inputs, arrow->e_right);
	    } else {
		AppendList(inputs, arrow->e_right->e_list);
	    }
	}
	arrow->e_type = T_SPECIAL;
    });
    RemoveScope();
}

	
PUBLIC void
Invoke(header, inputs, outputs)
struct expr *header;
struct slist *inputs, *outputs;
{
    struct job *inv_job, *save;
    struct invocation *inv;

    DBUG_ENTER("Invoke");
    inv_job = InvokationJob(header, inputs, outputs, &inv);
    FutureOutput(outputs, inv_job, inv->inv_scope_nr);

    save = InsertJob(inv_job);
    ReallyDoInvoke(&inv);
    DBUG_PRINT("job", ("invocation succeeded in one turn"));
    /* This happens when the action is a builtin command, or when the command
     * was cached
     */
    RemoveInsertedJob(save);
    DBUG_VOID_RETURN;
}

PUBLIC void
InvokeAfterwards(inv)
struct invocation *inv;
{
    struct job *save = InsertJob(inv->inv_job);

    DBUG_ENTER("InvokeAfterwards");
    ReallyDoInvoke(&inv);
    DBUG_PRINT("job", ("invocation afterwards succeeded in one turn"));
    RemoveInsertedJob(save);
    DBUG_VOID_RETURN;
}

PRIVATE void
RmInvokation(inv)
struct invocation *inv;
{
    put_expr_list(inv->inv_inputs);
    put_expr_list(inv->inv_outputs);
    free_invocation(inv);
}

PRIVATE char *
PrInvokation(inv)
struct invocation *inv;
{
    if (!IsEmpty(inv->inv_outputs)) {
	(void)sprintf(ContextString, "invocation of `%s' producing `%s'",
		     inv->inv_tool_id->id_text,
		     SystemName(ItemP(HeadOf(inv->inv_outputs), expr)->e_object));
    } else {
	(void)sprintf(ContextString, "invocation of `%s'",
		     inv->inv_tool_id->id_text);
    }
    return(ContextString);
}

PUBLIC void
InitInvokations()
{
    ClassInvocation = NewJobClass("invocation", (void_func) ReallyDoInvoke,
			 (void_func) RmInvokation, (string_func) PrInvokation);
}

PUBLIC long
InputSize(job)
struct job *job;
/* This function is called when a command has to be run, and there currently
 * is no (virtual) processor ready. It delivers the current size of the
 * inputs, which is used to determine the expected "best" place in the
 * ToDo list.
 */
{
    if (JobIndicator(job) == ClassInvocation) {
	/* we only know about tool invocation, here */
	struct invocation *inv = (struct invocation *)JobInfo(job);
	long total = 0;

	ITERATE(inv->inv_inputs, expr, obj_expr, {
	    total += GetObjSize(obj_expr->e_object);
	});
	return(total);
    } else {
	return(0);
    }
}

PUBLIC struct expr *InvCall(inv) struct invocation *inv;
{ return(inv->inv_call); }

PUBLIC struct slist **InvInputPtr(inv) struct invocation *inv;
{ return(&inv->inv_inputs); }

PUBLIC struct slist **InvOutputPtr(inv) struct invocation *inv;
{ return(&inv->inv_outputs); }

