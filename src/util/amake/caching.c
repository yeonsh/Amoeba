/*	@(#)caching.c	1.3	94/04/07 14:47:12 */
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
#include "main.h"
#include "slist.h"
#include "objects.h"
#include "expr.h"
#include "scope.h"
#include "type.h"
#include "builtin.h"
#include "dbug.h"
#include "Lpars.h"
#include "lexan.h"
#include "dump.h"
#include "symbol2str.h"
#include "execute.h"
#include "assignment.h"
#include "error.h"
#include "conversion.h"
#include "cluster.h"
#include "eval.h"
#include "invoke.h"
#include "os.h"
#include "tools.h"
#include "statefile.h"
#include "caching.h"

#if __STDC__
#define PROTO(x) x
#else
#define PROTO(x) ()
#endif

/* Local attributes in cache:
 * Give each cache entry its own scope number, starting from, say, 1000.
 * Attributes can then be attached as before! Only a special
 * GetCachedAttrValue may have to be provided, which looks at the
 * CacheAttributeScope first, which is set when a certain cache entry is
 * being handled.
 */

PUBLIC struct idf *Id_compare;

#define ID_SCOPE_NR	AttributeScopeNr /* was: GLOBAL_SCOPE */

#define CACHE_SCOPE_INIT	1000
/* assume we have less then CACHE_SCOPE_INIT clusters*/

PRIVATE int LastCacheScope = CACHE_SCOPE_INIT;
PUBLIC  int CacheChanged = FALSE;
PUBLIC  struct cached *FirstCacheEntry = NULL;

/* The cache entries can be found quickly because they are indexed on their
 * first input object. There's a seperate list for entries without inputs.
 */
PRIVATE struct cached *NoInputsCache = NULL;

PRIVATE struct slist *Intermediates = NULL;

PUBLIC int CacheScopeNr;

PRIVATE struct cached *new_cache_entry PROTO(( struct expr *inv ));
PRIVATE void BadStateEntry PROTO(( struct idf *idp ));
PRIVATE struct expr *ComputedArg PROTO(( struct slist *arg_list, struct idf *comp_id ));
PRIVATE struct expr *GetComputed PROTO(( struct slist *arg_list, struct idf *comp_id ));
PRIVATE void MarkComputed PROTO(( struct slist *arg_list, struct idf *comp_id, int scope ));
PRIVATE int NumberEqual PROTO(( struct expr *expr1, struct expr *expr2 ));
PRIVATE int ArgsMatch PROTO(( struct slist *args, struct slist *cached ));
PRIVATE struct object *FirstInput PROTO(( struct expr *call ));

PRIVATE struct cached *
new_cache_entry(inv)
struct expr *inv;
{
    struct cached *c = new_cached();

    c->c_invocation = inv;
    CacheScopeNr = c->c_scope_nr = LastCacheScope++;
    return(c);
}

PRIVATE struct slist *TouchList = NULL;

PUBLIC void
TouchFile(name)
char *name;
/* Causes Amake to suppose files with this last component are changed */
{
    Append(&TouchList, name);
}

PUBLIC void
CacheCommit()
{
    register struct cons *cons;
    register struct object *obj;

    /*
     * After the statefile has been read, and the cache initialised,
     * we know about all the relevant objects, so we can `touch' the
     * ones specified on the command line.
     */
    for (cons = Head(TouchList); cons != NULL; cons = Next(cons)) {
	register struct idf *ob_idp = str2idf((char*)Item(cons), 1);

	for (obj = ob_idp->id_object; obj != NULL; obj = obj->ob_same_comp) {
	    DBUG_PRINT("touch", ("`%s' touched", SystemName(obj)));
	    obj->ob_flags |= O_TOUCHED;
	}
    }

    if (F_globalize) {
	/* replace unused global cid attributes. */
	for (obj = FirstObject; obj != NULL; obj = obj->ob_next) {
	    if ((obj->ob_flags & O_STATE_INPUT) != 0 &&
		(obj->ob_flags & O_GLOBAL_CID) == 0) {
		GlobalizeAttr(obj, Id_cid);
	    }
	}
    }
}

PUBLIC int
RetainCacheEntry(c)
struct cached *c;
/* return TRUE iff this cache entry has to be written to the statefile */
{
    if (F_one_version && !Interrupted) {
	return((c->c_flags & (CH_WORKING|CH_NEW_ENTRY)) == 0 &&
	       (c->c_flags & CH_USED) != 0);
    } else {
	/* doesn't matter whether it's used or not in this run */
	return((c->c_flags & (CH_WORKING|CH_NEW_ENTRY)) == 0);
    }
}

PRIVATE void
BadStateEntry(idp)
struct idf *idp;
{
    if ((idp->id_flags & I_ERROR) == 0) {
	Warning("tool header of `%s' changed, cache entries ignored",
		idp->id_text);
	idp->id_flags |= I_ERROR;
    }
    CacheChanged = TRUE;
}


PUBLIC int
FileExists(obj)
struct object *obj;
/* Look if the file exists, by getting the `value id' of the file.
 * The result is added to the object, whenever possible, to avoid
 * having to stat/name_lookup more than necessary.
 */
{
    register struct expr *val;

    if ((val = GetAttrValue(obj, Id_id)) == UnKnown) {
	val = GetValueId(obj);
	if (val != NoValueId || GetAttrValue(obj, Id_target) == UnKnown) {
	    PutAttrValue(obj, Id_id, val, ID_SCOPE_NR);
	} else {
	    /* When the object is a target, it may become available at a later
	     * time, so don't set the Id_id attribute in that case.
	     */
	}
    }
    return(val != NoValueId);
}

PRIVATE struct expr *
ComputedArg(arg_list, comp_id)
struct slist *arg_list;
struct idf *comp_id;
{
    ITERATE(arg_list, expr, arrow, {
	if (arrow->e_left->e_param->par_idp == comp_id) {
	    assert(arrow->e_right->e_number == OBJECT);
	    return(arrow->e_right);
	}
    });
    /*NOTREACHED*/
}

PRIVATE struct expr *
GetComputed(arg_list, comp_id)
struct slist *arg_list;
struct idf *comp_id;
{
    return(GetExprOfType(GetContents(ComputedArg(arg_list, comp_id)->e_object,
				     TRUE, FALSE),
			 T_LIST_OF | T_OBJECT));
}

PRIVATE void
MarkComputed(arg_list, comp_id, scope)
struct slist *arg_list;
struct idf *comp_id;
int scope;
{
    PutAttrValue(ComputedArg(arg_list, comp_id)->e_object, Id_computed,
		 TrueExpr, scope);
}

PRIVATE int
NumberEqual(expr1, expr2)
struct expr *expr1, *expr2;
{
    if (IsList(expr1)) {
	if (IsList(expr2)) {
	    return(Length(expr1->e_list) == Length(expr2->e_list));
	} else {
	    return(FALSE);
	}
    } else {
	return(!IsList(expr2));
    }
}

PRIVATE int
ArgsMatch(args, cached)
/* Check if the tool-invocation has the same header (`args') as that of
 * the given cache-entry `cached'.
 * Assumption: the tool definition (header especially) hasn't changed between
 * two Amake runs.
 */
struct slist *args, *cached;
{
    register struct cons *acons, *ccons;

    for (acons = Head(args), ccons = Head(cached);
	 acons != NULL;
	 acons = Next(acons), ccons = Next(ccons)) {
	register struct expr *arrow = ItemP(acons, expr);
	register struct expr *carrow = ItemP(ccons, expr);

	if ((arrow->e_left->e_param->par_type->tp_indicator & T_OUT) != 0) {
	    /* check that the number of outputs is the same */
	    if (!NumberEqual(arrow->e_right, carrow->e_right)) {
		return(FALSE);
	    }
	} else if ((arrow->e_left->e_param->par_flags & PAR_COMPUTED) != 0) {
	    /* Computed inputs, change them to the one in the cache.
	     * Apparently the assumption here is that the call is cached.
	     * That's strange considering the fact that we are looking *if*
	     * it's cached!
	     */
	    put_expr(arrow->e_right);
	    arrow->e_right = CopyExpr(carrow->e_right);
	} else if (!is_equal(arrow->e_right, &carrow->e_right)) {
	    DBUG_PRINT("miss", ("parameter `%s' doesn't match", 
				arrow->e_left->e_param->par_idp->id_text));
	    return(FALSE);
	}
    }
    return(TRUE);
}

PRIVATE struct object *
FirstInput(call)
struct expr *call;
/* deliver first input object of invocation inv, or NULL */
{
    ITERATE(call->e_args, expr, arrow, {
	if ((arrow->e_left->e_param->par_type->tp_indicator & T_IN) != 0) {
	    register struct expr *arg = arrow->e_right;

	    if ((arg->e_type & T_LIST_OF) != 0) {
		if (!IsEmpty(arg->e_list)) {
		    return(ItemP(HeadOf(arg->e_list), expr)->e_object);
		}
	    } else {
		return(arg->e_object);
	    }
	}
    });
    return(NULL);
}

PRIVATE struct cached *
FindInCache(call)
struct expr *call;
{
    register struct object *first_input;
    register struct cached *c;

    /* Returns a pointer to the cached tool-invocation, with
     * the same parameters as `call' if present, otherwise return NULL.
     * The value-id's are not checked.
     * The cached invocations are indexed by their first input object.
     */
    
    if ((first_input = FirstInput(call)) != NULL) {
	DBUG_PRINT("first", ("first input is `%s'", SystemName(first_input)));
	c = first_input->ob_cache;
    } else {
	DBUG_PRINT("first", ("no first input!"));
	c = NoInputsCache;
    }

    for (; c != NULL; c = c->c_same_input) {
	register struct expr *inv = c->c_invocation;

	if (inv->e_func == call->e_func &&
	    ArgsMatch(call->e_args, inv->e_args)) {
	    
	    DBUG_PRINT("cache", ("cache hit!"));
	    return(c);
	}
    }

    DBUG_PRINT("cache", ("not in cache"));
    return(NULL);
}

PUBLIC struct expr *
QuickValueId(obj)
struct object *obj;
{
    register struct expr *res, *alias;

    if ((alias = GetAttrValue(obj, Id_alias)) != UnKnown) {
	DBUG_PRINT("stat", ("follow alias link for the id"));
	return(QuickValueId(alias->e_object));
    } else if ((res = GetAttrValue(obj, Id_id)) != UnKnown) {
	DBUG_PRINT("stat", ("id of `%s' was cached", SystemName(obj)));
	return(res);
    } else {
	DBUG_PRINT("stat", ("id of `%s' requires stat!", SystemName(obj)));
	res = GetValueId(obj);
	PutAttrValue(obj, Id_id, res, ID_SCOPE_NR);
	return(res);
    }
}

PRIVATE int
CheckObject(current, cached, inv_scope)
struct object *current, *cached;
int inv_scope;
/* Return the fact whether the object, which is used as input of an invocation,
 * stayed the same since the last Amake run.
 */
{
    struct expr *cur_id, *cache_cid;

    if ((cached->ob_flags & O_TOUCHED) != 0) {
	return(FALSE);
    }

    /* If the Id_id attribute of current in this scope isn't set, do it now.
     * This approach avoids a few expensive GetValueId's
     */
    if ((cur_id = GetAttrValue(current, Id_id)) == UnKnown) {
	struct expr *alias;

	if (GetAttrInScope(current, Id_is_source, inv_scope) == UnKnown &&
	    (alias = GetAttrInScope(current, Id_alias, inv_scope)) != UnKnown){
	    /* generated file, return the fact whether it has changed */
	    if (F_no_exec) {
		return(!IsTrue(GetAttrValue(alias->e_object,Id_changed)));
	    } else {
		cur_id = GetValueId(alias->e_object);
	    }
	} else {
	    cur_id = GetValueId(current);
	}
	PutAttrValue(current, Id_id, cur_id, ID_SCOPE_NR);
    }

    if ((cache_cid = GetAttrInScope(cached, Id_cid, CacheScopeNr)) == UnKnown){
	cache_cid = GetAttrInScope(cached, Id_cid, GLOBAL_SCOPE);
    }

    if (cache_cid == UnKnown) {
	/* this has happened several times, so check for it */
	Verbose(2, "`%s' fails cid attribute, considered out of date",
		SystemName(cached));
	return(FALSE); /* just redo the command */
    }

#ifdef STR_ID
    DBUG_PRINT("cid", ("id of `%s' is %s", SystemName(current),
		       do_get_string(cur_id)));
    DBUG_PRINT("cid", ("cid of `%s' is %s", SystemName(cached),
		       do_get_string(cache_cid)));

    return(strcmp(do_get_string(cache_cid), do_get_string(cur_id)) == 0);
#else
    cache_cid = GetInteger(cache_cid); /* possibly old cache */
    DBUG_PRINT("cid", ("id of `%s' is %ld", SystemName(current),
		       cur_id->e_integer));
    DBUG_PRINT("cid", ("cid of `%s' is %ld", SystemName(cached),
		       cache_cid->e_integer));
    return(cache_cid->e_integer == cur_id->e_integer);
#endif
}

PRIVATE int
SameInputs(cached, invocation, inv_scope)
struct cached *cached;
struct expr *invocation;
int inv_scope;
{
    struct expr *ch_inv = cached->c_invocation;
    register struct cons *ch_cons, *inv_cons;

    CacheScopeNr = cached->c_scope_nr;
    for (inv_cons = Head(invocation->e_args), ch_cons = Head(ch_inv->e_args);
	 inv_cons != NULL;
	 inv_cons = Next(inv_cons), ch_cons = Next(ch_cons)) {
	struct expr *iarrow = ItemP(inv_cons, expr),
		    *iarg = iarrow->e_right,
		    *carg = ItemP(ch_cons, expr)->e_right;

	if ((iarrow->e_left->e_param->par_type->tp_indicator & T_IN) != 0) {
	    if ((iarg->e_type & T_LIST_OF) != 0) {
		register struct cons *ccons, *icons;

		for (icons = Head(iarg->e_list), ccons = Head(carg->e_list);
		     icons != NULL;
		     icons = Next(icons), ccons = Next(ccons)) {
		    if (!CheckObject(ItemP(icons, expr)->e_object,
				     ItemP(ccons, expr)->e_object,
				     inv_scope)) {
			Verbose(1, "`%s' is out of date",
				SystemName(ItemP(icons, expr)->e_object));
			return(FALSE);
		    }
		}
	    } else {
		assert(iarg->e_type == T_OBJECT && carg->e_type == T_OBJECT);
		if (!CheckObject(iarg->e_object, carg->e_object, inv_scope)) {
		    Verbose(1, "`%s' is out of date",
			    SystemName(iarg->e_object));
		    return(FALSE);
		}
	    }
	}
    }
    return(TRUE);
}

PRIVATE void
DoSetValueId(obj, new)
struct object *obj;
int new;
{
    register struct expr *cid;

    /* watch out: we get the GLOBAL value of cid here (instead of the one
     * in CacheScopeNr).
     */
    cid = GetAttrInScope(obj, Id_cid, GLOBAL_SCOPE);

    if (new && cid == UnKnown) {
	/* first time this object is used as input */
	PutAttrValue(obj, Id_cid, QuickValueId(obj), GLOBAL_SCOPE);
    } else {
	/* old cache entry or cid attribute already globally set, which
	 * is caused by another invocation having the same object as input.
	 */
	struct expr *id = QuickValueId(obj);

	/* set it locally if it is really different, or
	 * when it has been set before in CacheScopeNr (!)
	 */
	if (!is_equal(cid, &id) ||
	    GetAttrInScope(obj, Id_cid, CacheScopeNr) != UnKnown) {
	    PutAttrValue(obj, Id_cid, id, CacheScopeNr);
	}
    }
}

#define ITER_ARG(arrow, argp, statement)	\
{						\
    register struct expr **argp;		\
						\
    if (arrow->e_right->e_type & T_LIST_OF) {	\
	register struct cons *c;		\
						\
	for (c = Head(arrow->e_right->e_list); c != NULL; c = Next(c)) { \
		argp = (struct expr**)&Item(c);	\
		statement;			\
	}					\
    } else {					\
	argp = &arrow->e_right;			\
        statement;				\
    }						\
}

#define ITER_ARGS(call, mask, item, statement)			\
{								\
    register struct cons *c;					\
								\
    for (c = Head(call->e_args); c != NULL; c = Next(c)) {	\
	register struct expr *item, *c_arrow = ItemP(c, expr);	\
								\
	if (c_arrow->e_left->e_param->par_type->tp_indicator & (mask)) { \
	    register struct expr *c_arg = c_arrow->e_right;	\
								\
	    if (c_arg->e_type & T_LIST_OF) {			\
		ITERATE(c_arg->e_list, expr, item, statement);	\
	    } else {						\
	        item = c_arrow->e_right;			\
	        statement;					\
	    }							\
	}							\
    }								\
}


PRIVATE void
EvalInput(arrow)
struct expr *arrow;
{
    /* Objects are a bit special, because we try to optimise the usage
     * of `cid' attributes. Whenever possible, the *global* value should
     * be used, in order to void multiple local occurences in the Statefile.
     */
    register int no_attr;

    ITER_ARG(arrow, argp, {
	no_attr = (*argp)->e_number != '[';
	*argp = GetObject(Eval(*argp));
	if (no_attr) {
	    (*argp)->e_object->ob_flags |= (O_GLOBAL_CID | O_STATE_INPUT);
	    DBUG_PRINT("global", ("global cid of `%s' used",
				  SystemName((*argp)->e_object)));
	} else {
	    (*argp)->e_object->ob_flags |= O_STATE_INPUT;
	}
    });
}

PRIVATE void
AddToCache(c)
struct cached *c;
{
    register struct object *first_input;

    /* put in global cache list */
    c->c_next = FirstCacheEntry;
    FirstCacheEntry = c;

    /* update the index structure */
    if ((first_input = FirstInput(c->c_invocation)) != NULL) {
	c->c_same_input = first_input->ob_cache;
	first_input->ob_cache = c;
    } else {
	c->c_same_input = NoInputsCache;
	NoInputsCache = c;
    }
}

PUBLIC void
EnterStateEntry(inv_result)
struct expr *inv_result;
{
    register struct expr *inv;
    struct cached *new_entry;

    /* a cached invocation is of the form   `tool(params)==result' */
    if (inv_result->e_number != EQ) {
	Warning("ignored bad statefile entry");
	CacheChanged = TRUE;
	return;
    }

    inv = inv_result->e_left;
    new_entry = new_cache_entry(inv);

    if (inv->e_type == T_ERROR) { /* error found while parsing */
	goto BadEntry;
    }

    /* create objects and add the cid attributes */
    ITERATE(inv->e_args, expr, arrow, {
	int type = arrow->e_left->e_param->par_type->tp_indicator;

	if (arrow->e_right == NULL) {
	    goto BadEntry;
	} else {
	    if (F_globalize && (type & T_IN) != 0) {
		EvalInput(arrow);
	    } else {
		if ((arrow->e_right = GetExprOfType(Eval(arrow->e_right),
						    type & T_TYPE)) == NULL) {
		    goto BadEntry;
		}
	    }
	    if (IsShared(arrow->e_right)) {
		/* Marking it now will make sure that it stays shared,
		 * even when the tool flags are changed.
		 */
		SharedExpressionUsed(arrow->e_right);
	    }
	}
    });
    new_entry->c_result = inv_result->e_right;
    AddToCache(new_entry);
    return;

BadEntry:
    CacheChanged = TRUE;
    BadStateEntry(inv->e_func);
    free_cached(new_entry);
    return;
}
								    
PRIVATE void
SetValueIds(call, new)
struct expr *call;
int new; /* cache entry totally new? */
{
    /* The value id are already known in principle: they are now attached
     * (as Id_id) to the last invocation, which caused this cache update.
     * It is advisable to implement this, because stat()'s are slow.
     */
    ITER_ARGS(call, T_IN, obj, { DoSetValueId(obj->e_object, new); });
}


PRIVATE void Delete(obj)
struct object *obj;
{
    DBUG_PRINT("cache", ("remove `%s'", SystemName(obj)));

    if (!F_no_exec) {
	DoDelete(SystemName(obj), NON_FATAL);
    }

    /* the `current alias' and the `id' attributes must be invalidated */
    obj->ob_calias = NULL;
    PutAttrValue(obj, Id_id, UnKnown, ID_SCOPE_NR); /* all scopes ? */
}

PRIVATE struct cached *
PutInCache(call, job)
struct expr *call;
struct job *job;
{
    struct cached *c;

    /* Replace output objects by a variant (e.g. suffix it with _<num>).
     * A free <num> can be determined by sequentially searching in the
     * object space for obj_0, obj_1 until a new one is found.
     * Note that this may be a bit too strict, but that doesn't matter.
     * We could of course look in the pool itself, but this is much slower.
     */
    CacheChanged = TRUE;
    DBUG_EXECUTE("cache", {
	DBUG_WRITE(("put in cache:")); DBUG_Expr(call); DBUG_NL();
    });
    c = new_cache_entry(CopyExpr(call));

    ITERATE(c->c_invocation->e_args, expr, arrow, {
	register int type = arrow->e_left->e_param->par_type->tp_indicator;

	if ((type & T_OUT) != 0) {
	    register struct object *obj;

	    if ((type & T_LIST_OF) != 0) {
		register struct cons *acons;

		if (IsShared(arrow->e_right)) { /* we are changing it! */
		    arrow->e_right = CopySharedExpr(arrow->e_right);
		}
		for (acons = Head(arrow->e_right->e_list);
		     acons != NULL; acons = Next(acons)) {
		    obj = ItemP(acons, expr)->e_object;
		    Delete(obj);
		    Item(acons) = (generic) ObjectExpr(Variant(obj));
		}
	    } else {
		obj = arrow->e_right->e_object;
		Delete(obj);
		arrow->e_right = ObjectExpr(Variant(obj));
	    }
	}
    });
    c->c_working = job;
    AddToCache(c);
    return(c);
}

PUBLIC void
RemoveOutputs(inv)
struct expr *inv;
{
    ITER_ARGS(inv, T_OUT, obj, { Delete(obj->e_object); });
}

PUBLIC void
LookInCache(inv)
struct invocation *inv;
{
    struct cached *cached;

    if (inv->inv_cache == NULL) { /* so we're not suspended on the trigger */

	/* first instantiate the trigger, if available */
	if (inv->inv_call->e_func->id_tool->tl_trigger != NULL) {
	    AddScope(inv->inv_call->e_func->id_tool->tl_scope);
	    PutParams(inv->inv_call);
	    inv->inv_trigger =
		CopyExpr(inv->inv_call->e_func->id_tool->tl_trigger);
	    RemoveScope();
	}

	if ((cached = inv->inv_cache = FindInCache(inv->inv_call)) != NULL) {
	    if (inv->inv_trigger != NULL) {
		cached->c_flags |= CH_RERUN | CH_WORKING;
	    } else if ((cached->c_flags & CH_WORKING) != 0) {
		cached->c_flags |= CH_PICKUP;
		DBUG_PRINT("suspend", ("tool computing cache entry"));
		SuspendCurrent(cached->c_working);
		ReSched();
		/*NOTREACHED*/
	    } else if (IsFalse(cached->c_result) ||
		       !SameInputs(cached, inv->inv_call, inv->inv_scope_nr)) {
		inv->inv_trigger = TrueExpr;
		cached->c_flags = CH_RERUN | CH_WORKING;
	    } else {
		inv->inv_trigger = FalseExpr;
		cached->c_flags |= CH_CACHED;
	    }
	} else {
	    if (inv->inv_trigger == NULL) {
		inv->inv_trigger = TrueExpr;
	    }
	    cached = inv->inv_cache = PutInCache(inv->inv_call, inv->inv_job);
	    cached->c_flags = CH_NEW_ENTRY | CH_WORKING;
	}
    }

    inv->inv_state = INV_TRIGGER; /* no picking up */
    inv->inv_trigger = GetExprOfType(Eval(inv->inv_trigger), T_BOOLEAN);

    if (IsTrue(inv->inv_trigger)) {
	Verbose(4, "trigger is true");
	/* instantiate action */
	AddScope(inv->inv_call->e_func->id_tool->tl_scope);
	PutParams(inv->inv_call);
	inv->inv_action = CopyExpr(inv->inv_call->e_func->id_tool->tl_action);
	RemoveScope();
	
	if ((cached->c_flags & CH_RERUN) != 0) {
	    RemoveOutputs(inv->inv_call);
	}
    } else {
	Verbose(4, "trigger is false");
	/* invocation not triggered, put dummy action */
	inv->inv_action = TrueExpr;
    }
}


PUBLIC void
RemoveIntermediates()
{
    if (F_no_exec) return;

    ITERATE(Intermediates, object, obj, {
	/* Don't remove links more than once, and don't remove installed
	 * targets. Subtargets are also not removed, currently.
	 */
	if (obj->ob_calias != NULL &&
	    GetAttrValue(obj, Id_installed) == UnKnown
	    /* && !IsTrue(GetAttrValue(obj, Id_failed) */
	    /* uncommenting this check causes tainted files to be retained */
	    ) {
	    Delete(obj);
	}
    });
    RemoveDiagFiles();
}

/* Even if we optimise link()s, sometimes an object has to be
 * available in the current working directory.
 * A notable example is the case of generated C include files.
 * The attribute "Id_include" is used to detect this situation.
 */
#define MustLink(obj)	\
    (!F_no_exec &&	\
     (!F_link_opt  || IsTrue(GetAttrInScope(obj, Id_include, GLOBAL_SCOPE))))

PUBLIC void
InstallGeneratedInput(wanted, cached)
struct object *wanted, *cached;
{
    char wanted_path[256]; /* SystemName() overwrites a static buffer */
    char *cached_path;

    (void)strcpy(wanted_path, SystemName(wanted));
    DBUG_PRINT("cache", ("InstallGeneratedInput(%s, %s)",
			 wanted_path, SystemName(cached)));

    if (wanted->ob_calias == cached) {
	DBUG_PRINT("inv", ("`%s' was already installed", wanted_path));
    } else {
	DBUG_PRINT("cache", ("`%s' will be (re)installed", wanted_path));
	if (MustLink(wanted)) {
	    cached_path = SystemName(cached);
	    /* DoLink unlink()s if necessary, so no need to do this now. */
	    if (DoLink(cached_path, wanted_path) != 0) {
		/* this is undesireble, obviously but Fatal? */
		Warning("couldn't link `%s' to `%s'",
			wanted_path, cached_path);
	    }
	    if (!F_keep_intermediates && F_link_opt) {
		/* Correction (see below): we *do* have to remove it */
		Append(&Intermediates, wanted);
	    }
	}

	wanted->ob_calias = cached; /* set current alias */
    }
}

PRIVATE int
Install(inv_idp, wanted, cached, pickup, scope_nr, ok)
struct idf *inv_idp;
struct object *wanted, *cached;
int pickup, scope_nr, ok;
{
    /* SystemName() currently overwrites a static buffer, so be careful */
    int err = 0;
    char wanted_path[256], cached_path[256]; 

    if ((wanted->ob_flags & O_DIAG) != 0) {
	DBUG_PRINT("cache", ("don't install diag"));
	return ok;
    }

    (void)strcpy(wanted_path, SystemName(wanted));
    (void)strcpy(cached_path, SystemName(cached));

    DBUG_PRINT("cache", ("install `%s' as `%s' for scope %d",
			 wanted_path, cached_path, scope_nr));

    /* Only throw intermediate away when mustn't be explicitly kept,
     * and when we aren't using link() optimisation on a cached invocation.
     * However, if it turns out we *do* need the object in the cwd
     * after all, we'll have to correct it as soon as we find out (see above).
     */
    if (!F_keep_intermediates && !(F_link_opt && pickup)) {
	Append(&Intermediates, wanted);
    }

    PutAttrValue(wanted, Id_alias, ObjectExpr(cached), scope_nr);

    if (!pickup && ok) {
	int may_exist =	GetAttrInScope(cached, Id_cid,CacheScopeNr) != UnKnown,
	    equal;
	struct expr *new_id;

	if (may_exist && IsTrue(GetAttrValue(wanted, Id_compare))) {
	    if (F_no_exec) {
		/* If the compare attribute is set, most of the time nothing
		 * has changed, that's the reason the attribute was introduced
		 * in the first place!
		 */
		equal = TRUE;
	    } else {
		/* Look if the file *really* has changed, by means of bitwise
		 * comparison. This is great for generated header files.
		 */
		equal = FilesEqual(wanted, cached);
	    }
	} else {
	    PrintObject(wanted);
	    equal = FALSE;
	}

	if (F_no_exec) {
	    PutAttrValue(cached, Id_changed,
			 equal ? FalseExpr : TrueExpr, GLOBAL_SCOPE);
	} else {
	    if (equal) {
		/* restore the link to the one in the cache */
		DoDelete(wanted_path, FATAL);
		Verbose(1, "`%s' unchanged", wanted_path);
		wanted->ob_calias = NULL;	/* we destroyed the link */
	    } else {
		if (may_exist) {
		    DoDelete(cached_path, NON_FATAL);
		}
		if ((wanted->ob_flags & O_ABSENT_OK) == 0 &&
		    DoLink(wanted_path, cached_path) != 0) {
		    Warning("tool %s failed to produce file `%s'",
			    inv_idp->id_text, wanted_path);
		    err++;
		} else {
		    new_id = GetValueId(cached);
		    PutAttrValue(cached, Id_cid, new_id, CacheScopeNr);
		    PutAttrValue(cached, Id_id, new_id, ID_SCOPE_NR);
		}
	        wanted->ob_calias = cached; /* this version is now linked */
	    }
	}
    }

    if (!ok || err) {	/* warn consequent tools about this failure  */
	Verbose(3, "`%s' becomes tainted", cached_path);
	Taint(cached);
    }

    /* if (!ok) then keep the object, the user may still want to look at it.
     * Or should we put this under an option?
     */

    if (GetAttrValue(wanted, Id_target) != UnKnown) {
	/* Maybe the user has removed a target object, but the tool hasn't
	 * been run.  We have to reinstall the target in that case.
	 * Another reason for reinstalling it, is when the link has just
	 * been thrown away, because the object turned out to be the same
	 * as one in a previous invocation.
	 */
	if (!F_no_exec && ok && (pickup || wanted->ob_calias == NULL)) {
	    struct expr
		*old_id = GetValueId(wanted),
		*cache_id = GetAttrInScope(cached, Id_cid,CacheScopeNr);

	    if (!is_equal(old_id, &cache_id)) {
		if (old_id != NoValueId) {
		    DoDelete(wanted_path, FATAL);
		}
		if (DoLink(cached_path, wanted_path) != 0) {
		    Warning("couldn't reinstall `%s'", wanted_path);
		    err++;
		} else {
		    if (wanted->ob_calias != NULL) {
			Warning("reinstalled `%s'", wanted_path);
		    } else {
			wanted->ob_calias = cached;
		    }
		}
	    }
	}
	PutAttrValue(wanted, Id_target, UnKnown, GLOBAL_SCOPE);
	PutAttrValue(wanted, Id_installed, TrueExpr, GLOBAL_SCOPE);
	/* The cluster may be finished before someone uses its targets.
	 * If we don't give it free now, that job will be suspended for ever!
	 */
    }

    return(ok && err == 0);
}

PUBLIC void
HandleOutputs(invocation, cached, result, scope_nr)
struct expr *invocation;
struct cached *cached;
struct expr **result;
int scope_nr;
{
    struct expr *ch_inv = cached->c_invocation;
    register struct cons *ch_cons, *inv_cons;
    int pickup, ok, new;

    CacheScopeNr = cached->c_scope_nr;
    if (cached->c_flags & CH_CACHED) {
	cached->c_flags &= ~CH_CACHED;
	pickup = TRUE;
    } else if (cached->c_flags & (CH_RERUN | CH_NEW_ENTRY)) {
	CacheChanged = TRUE;
	if ((cached->c_flags & CH_RERUN) != 0) {
	    put_expr(cached->c_result);
	}
	cached->c_result = CopyExpr(*result);
	new = (cached->c_flags & CH_NEW_ENTRY) != 0;
	cached->c_flags &= ~(CH_RERUN | CH_NEW_ENTRY);

	pickup = FALSE;
    } else {
	assert(cached->c_flags & CH_PICKUP);
	/* don't clear CH_PICKUP flag, more than one may be waiting */
	pickup = TRUE;
	*result = CopyExpr(cached->c_result);
    }
    ok = !IsFalse(cached->c_result);

    for (inv_cons = Head(invocation->e_args), ch_cons = Head(ch_inv->e_args);
	 inv_cons != NULL;
	 inv_cons = Next(inv_cons), ch_cons = Next(ch_cons)) {
	struct expr *iarrow = ItemP(inv_cons, expr),
		    *iarg = iarrow->e_right;
	struct expr *carrow = ItemP(ch_cons, expr),
		    *carg = carrow->e_right;

	if ((iarrow->e_left->e_param->par_type->tp_indicator & T_OUT) != 0) {
	    /* only install outputs */
	    if ((iarg->e_type & T_LIST_OF) != 0) {
		register struct cons *ccons, *icons;

		for (icons = Head(iarg->e_list), ccons = Head(carg->e_list);
		     icons != NULL;
		     icons = Next(icons), ccons = Next(ccons)) {
		    ok = Install(invocation->e_func,
				 ItemP(icons, expr)->e_object,
				 ItemP(ccons, expr)->e_object,
				 pickup, scope_nr, ok);
		}
	    } else {
		ok = Install(invocation->e_func,
			     iarg->e_object, carg->e_object, pickup,
			     scope_nr, ok);
	    }
	} else if ((carrow->e_left->e_param->par_flags & PAR_COMPUTED) != 0) {
	    /* Update the `computed inputs' parameter, when the tool has been
	     * (re-)run.
	     */
	    if (!pickup) {
		put_expr(carg);
		carrow->e_right = GetComputed(ch_inv->e_args,
				       carrow->e_left->e_param->par_computed);
	    }
	    MarkComputed(invocation->e_args,
			 carrow->e_left->e_param->par_computed, scope_nr);
	    ITERATE(carrow->e_right->e_list, expr, obj_expr, {
		/* mark the objects that really have been included, so
		 * that we can complain about the ones who didn't,
		 * after the cluster has been done.
		 */
		PutAttrValue(obj_expr->e_object, Id_computed, TrueExpr, 
			     scope_nr);
		PutAttrValue(obj_expr->e_object, Id_implicit, TrueExpr,
			     GLOBAL_SCOPE);
	    });
	}
    }

    if (!ok && IsTrue(cached->c_result)) {
	/* Even though the exit status indicated a succesful run,
	 * one or more expected output objects were missing.
	 * Overwrite the result of this invocation, so that we
	 * will retry it in a new Amake run.
	 */
	cached->c_result = FalseExpr;
    }

    cached->c_flags |= CH_USED;
    /* unused cache entries are not written with the F_one_version flag on */

    if (!pickup) { /* now it is ready to be dumped */
	cached->c_flags &= ~CH_WORKING;

	/* After the invocation has been done we know what the REAL inputs
	 * were, so now we can get all necessary input value-ids.
	 */
	SetValueIds(ch_inv, new);
    }

    if (!pickup || Recovering) {
	/* Updated or new cache entries are dumped relatively soon.
	 * Cached ones are appended when all has been done.
	 * If we are recovering however, we immediately try to generate
	 * a new Statefile, also containing cached entries.
	 */
	DumpCacheEntry(cached);
    }
}

PUBLIC void
InitCaching()
{
    DeclareAttribute(Id_compare = str2idf("compare", 0));
    Id_compare->id_flags |= F_SPECIAL_ATTR;
    /* Name of attribute that causes output comparison by Amake.
     * This may save us a lot of recompilations.
     */
}

