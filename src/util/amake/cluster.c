/*	@(#)cluster.c	1.3	94/04/07 14:47:50 */
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
#include "parser.h"
#include "conversion.h"
#include "tools.h"
#include "scope.h"
#include "invoke.h"
#include "main.h"
#include "caching.h"
#include "declare.h"
#include "os.h"
#include "assignment.h"
#include "cluster.h"
#include "statefile.h"
#include "docmd.h"
#include "classes.h"

PUBLIC struct param *CurrentParameter; /* of tool under consideration */

PRIVATE int ClassCluster; /* Class no. of cluster jobs */

PUBLIC struct cluster *CurrentCluster = NULL;
/* during the tool determination part of the cluster algorithm this points
 * the current cluster being run. This is used in handling computed
 * inputs (`mkdep' and the like).
 */

PRIVATE struct slist
    *ClustersOrObjectsToDo = NULL,
    *AllClusters = NULL;

PRIVATE struct slist *DirAttribute = NULL; /* only will contain Id_dir */

PUBLIC struct idf *Id_persistent;
/* Used to indicate that the object should not be removed from the sources
 * after being used as (explicit) input.
 */

#define MARGIN		2
#define LINE_LENGTH	80

PRIVATE void
ObjectsOnError(list)
struct slist *list;
{
    if (!IsEmpty(list)) {
	if (Head(list) == Tail(list)) { /* only one object */
	    PrintError("%s", SystemName(ItemP(HeadOf(list), expr)->e_object));
	} else {
	    int printed = MARGIN;

	    PrintError("{ ");
	    ITERATE(list, expr, obj, {
		char *name = SystemName(obj->e_object);
		int len = strlen(name);
		int last_one = Next(cons) == NULL;
		
		if ((printed + len) > (LINE_LENGTH - 2)) {
		    PrintError("\n  ");
		    printed = MARGIN;
		}
		PrintError("%s%s ", name, last_one ? "" : ",");
		printed += len + 2;
	    });
	    PrintError("}");
	}
    }
}

PRIVATE void
PrintCluster(clus)
struct cluster *clus;
{
    if ((clus->cl_flags & C_ANONYMOUS) != 0) {
	PrintError("cluster producing ");
	ObjectsOnError(clus->cl_orig_targets->e_list);
    } else {
	PrintError("cluster `%s'", clus->cl_idp->id_text);
    }
}

/*
 * Cluster determination: avoid running clusters unrelated to the requested
 * ones. The algorithm is as follows:
 *
 * - First select only the clusters that may potentially be run, i.e.,
 *  (1)    (a) the clusters requested on the commandline
 *      or (b) the clusters marked as default
 *      or (c) clusters producing objects marked as default
 *      or (d) the first one mentioned in the Amakefile
 *
 *  (2) unnamed clusters, which are not supposed to be requested directly by
 *      the user, because they generate a subtarget.
 *
 * - Check if some of the sources of a cluster are used as a target elsewhere
 *   (this can be done because all clusters have had their chance
 *   to set the appropriate Id_target attributes.) If this is
 *   the case, that cluster is marked with C_DO_RUN, and checked recursively.
 *
 * - Finally all clusters are awoken, and each one checks for itself if it has
 *   to run.
 */

PRIVATE void VisitCluster( /* struct cluster *c, struct slist *potential */ );

PRIVATE void
DoVisit(src, clus_names)
struct object *src;
struct slist *clus_names;
{
    register struct cons *cons;
    struct cluster *target_of = NULL;
    struct expr *target_clus;

    for (cons = Head(clus_names); cons != NULL; cons = Next(cons)) {
	register struct expr *clus_id = ItemP(cons, expr);
	register struct cluster *clus = clus_id->e_idp->id_cluster;

	if ((clus->cl_flags & C_POTENTIAL) != 0) {
	    /* only targets of potential clusters matter */
	    if (target_of == NULL) {
		target_of = clus;
	    } else {
		RunTimeError("`%s' both target of `%s' and `%s'",
			     SystemName(src), target_of->cl_idp->id_text,
			     clus->cl_idp->id_text);
	    }
	}
	if (target_of != NULL) {
	    struct expr *cl_id = new_expr(ID);
	    cl_id->e_idp = target_of->cl_idp;
	    target_clus = empty_list();
	    Append(&target_clus->e_list, cl_id);
	} else {
	    target_clus = UnKnown;
	}
	PutAttrValue(src, Id_target, target_clus, GLOBAL_SCOPE);
	if (target_of != NULL) {
	    VisitCluster(target_of);
	}
    }
}

PRIVATE void
VisitCluster(c)
struct cluster *c;
{
    struct expr *targets;

    if ((c->cl_flags & C_VISITED) != 0) {
	DBUG_PRINT("visit", ("`%s' already visited", c->cl_idp->id_text));
	return;
    } else {
	c->cl_flags |= (C_VISITED | C_DO_RUN);
    }
    
    ITERATE(c->cl_sources->e_list, expr, src, {
	if ((targets = GetAttrValue(src->e_object, Id_target)) != UnKnown) {
	    /* target attributes may be reset, in which case the old value
	     * is thrown away. So copy it first, and throw away afterwards.
	     */
	    targets = CopyExpr(targets);
	    DoVisit(src->e_object, targets->e_list);
	    put_expr(targets);
	}
    });
}

PRIVATE struct cluster *
DetermineCluster(obj)
struct object *obj;
{
    if ((obj->ob_idp->id_kind & I_CLUSTER) != 0) {
	/* is a reference to a cluster instead of to an object */
	return(obj->ob_idp->id_cluster);
    } else {	/* supposed to be the target of some cluster, check */
	struct expr *targets;

	if ((targets = GetAttrValue(obj, Id_target)) != UnKnown) {
	    assert(!IsEmpty(targets->e_list));
	    if (Head(targets->e_list) != Tail(targets->e_list)) {
		Warning("`%s' target of more than one cluster",
			SystemName(obj));
	    }
	    return(ItemP(HeadOf(targets->e_list), expr)->e_idp->id_cluster);
	} else {
	    Warning("`%s' not the target of any cluster (ignored)",
		    SystemName(obj));
	    return(NULL);
	}
    }
}

PRIVATE struct slist *
PotentialClusters(clus_or_objs, defaults, all)
struct slist *clus_or_objs;
struct slist *defaults;
struct slist *all;
{
    struct slist *todo = NULL;

    if (!IsEmpty(clus_or_objs)) {	/* do clusters on the commandline */
	ITERATE(clus_or_objs, expr, obj_expr, {
	    struct cluster *clus = DetermineCluster(obj_expr->e_object);

	    if (clus != NULL) {
		clus->cl_flags |= C_POTENTIAL;
		Append(&todo, clus);
	    }
	});
    } else if (!IsEmpty(defaults)) {
	ITERATE(defaults, expr, obj_expr, {
	    struct cluster *clus = DetermineCluster(obj_expr->e_object);

	    if (clus != NULL) {
		clus->cl_flags |= C_POTENTIAL;
		Append(&todo, clus);
	    }
	});
    } else if (!IsEmpty(all)) {	/* by default, add the first cluster */
	struct cluster *first = ItemP(HeadOf(all), cluster);

	first->cl_flags |= C_POTENTIAL;
	Append(&todo, first);
    }

    /* and add all anonymous clusters */
    ITERATE(all, cluster, clus, {
	if ((clus->cl_flags & C_ANONYMOUS) != 0) {
	    clus->cl_flags |= C_POTENTIAL;
	}
    });

    return(todo);
}	    

PRIVATE void
DoSynchronise()
{
    struct slist *ClustersToDo = NULL;

    if (IsEmpty(AllClusters)) return;

    ClustersToDo =
	PotentialClusters(ClustersOrObjectsToDo, GetDefaults(), AllClusters);
    
    if (F_verbose) {
	if (Verbosity > 3) {
	    PrintError("potential clusters:\n");
	    ITERATE(AllClusters, cluster, c, {
		if ((c->cl_flags & C_POTENTIAL) != 0) {
		    PrintCluster(c); NewLine();
		}
	    });
	}
	if (Verbosity > 1) {
	    PrintError("clusters to be done:\n");
	    ITERATE(ClustersToDo, cluster, c, {
		PrintCluster(c); NewLine();
	    });
	}
    }

    ITERATE(ClustersToDo, cluster, c, {
	VisitCluster(c);
    });
}

PRIVATE void
AwakeClusters(list)
struct slist *list;
{
    ITERATE(list, cluster, clus, Awake(clus->cl_job, (struct job *)NULL));
}

PRIVATE void
MarkSources(clus)
struct cluster *clus;
/*
 * When objects are given as input to tools, they will be (re-)ordered
 * according to the ordering of the %sources (if any) they stem from.
 * To be able to achieve this, the source objects are marked with a sequence
 * number. When a tool uses marked objects as input, the output object(s)
 * will inherit this sequence number, so that they can be used by subsequent
 * tools as well.
 * An alternative would be to keep a `stems-from' attribute which can be used
 * to search for the sequence number(s) of its original source(s). This will
 * probably be slower, however.
 *
 * Furthermore, the is-source attribute on the sources is set to %true.
 * They may be used to resolve some difficult conflicts in building
 * the tool hierarchy
 */
{
    long seq_nr = 0;
    register struct cons *cons, *next;

    for (cons = Head(clus->cl_sources->e_list); cons != NULL; ) {
	register struct object *obj = ItemP(cons, expr)->e_object;

	next = Next(cons);
	DeclObjectIsSource(obj);
	PutAttrValue(obj, Id_seq_nr, IntExpr((long)seq_nr++),
		     clus->cl_scope->sc_number);
	if (IsTrue(GetAttrInScope(obj, Id_is_source,
				  clus->cl_scope->sc_number))) {
	    Warning("`%s' appears as source more than once", SystemName(obj));
	    Remove(&clus->cl_sources->e_list, cons);
	} else {
	    PutAttrValue(obj, Id_is_source, TrueExpr,
			 clus->cl_scope->sc_number);
	}
	cons = next;
    }
}

PRIVATE void
MarkTargets(clus)
struct cluster *clus;
{
    ITERATE(clus->cl_targets->e_list, expr, obj_expr, {
	register struct object *obj = obj_expr->e_object;
	register struct expr *clus_id;

	clus_id = new_expr(ID);
	clus_id->e_idp = clus->cl_idp;
	AppendAttrValue(obj, Id_target, clus_id, GLOBAL_SCOPE);
    });
}

PRIVATE int
    NrClustersRead = 0,
    NrClusters = 1000,	/* assume there are a lot of clusters */
    NrSync = 0;		/* no. of clusters that tried to synchronize */

PRIVATE struct slist *WaitingClusters = NULL;

PUBLIC void
AllClustersKnown()
{
    NrClusters = NrClustersRead;
    ITERATE(AllClusters, cluster, c, PutInReadyQ(c->cl_job));
}

PRIVATE void
Synchronisation(clus)
struct cluster *clus;
{
    DBUG_ENTER("Synchronisation");

    DBUG_EXECUTE("objects", {
	DBUG_WRITE(("sources:")); DBUG_NL();
	ITERATE(clus->cl_sources->e_list, expr, obj, {
	    PrintObject(obj->e_object);
	});
    });
    DBUG_EXECUTE("objects", {
	DBUG_WRITE(("targets:")); DBUG_NL();
	ITERATE(clus->cl_targets->e_list, expr, obj, {
	    PrintObject(obj->e_object);
	});
    });

    if (++NrSync < NrClusters) {
	Append(&WaitingClusters, clus);
	DBUG_PRINT("suspend", ("cluster synchronisation"));
	SuspendCurrent((struct job *)NULL); /* suspend till explicit Awake */
	ReSched();/*NOTREACHED*/
    } else {
	assert(NrSync == NrClusters);
	AwakeClusters(WaitingClusters);
	DoSynchronise();
    }
    DBUG_VOID_RETURN;
}

PRIVATE struct expr *
FindFiles(clus, io_type, attr_spec, del_sources, del_targets, del_persistent)
struct cluster *clus;
int io_type;
struct expr *attr_spec;
struct slist **del_sources, **del_targets, **del_persistent;
/*
 * Look for sources or targets (depends on io_type) which match the attributes
 * in attr_spec. The objects found are removed from the cluster, and returned.
 * A copy of them is added to del_sources or del_targets respectively.
 */
{
    struct slist **obj_list, **delete;
    register struct cons *cons, *next, *acons;
    struct expr *val, *obj;
    struct idf *attr;
    struct slist *found = NULL;
    int persistent_seen = FALSE;
    int find_all = (io_type & T_LIST_OF) != 0;

    DBUG_ENTER("FindFiles");
    
    if ((io_type & (T_IN|T_OUT)) == T_IN) {
	obj_list = &clus->cl_sources->e_list;
	delete = del_sources;
    } else {
	assert((io_type & T_OUT) != 0);
	obj_list = &clus->cl_targets->e_list;
	delete = del_targets;
    }
    for (cons = Head(*obj_list); cons != NULL; cons = next) {
	next = Next(cons);
	obj = ItemP(cons, expr);
	
	for (acons = Head(attr_spec->e_list); acons != NULL;
	     acons = Next(acons)) {
	    register struct expr *spec = ItemP(acons, expr);

	    attr = get_id(spec->e_left);
	    if ((attr->id_flags & F_SPECIAL_ATTR) != 0) {
		 /* don't check special attributes */
		 if (attr == Id_persistent) {
		     persistent_seen = TRUE;
		 } else if (attr == Id_implicit) {
		     if (del_persistent != NULL) {
			 /* This is the cc-c/mkdep case, so don't add it */
			 break;
		     } else {
			 /* This is the case that the cc-c has a built-in
			  * mkdep. We are then looking for the include files
			  * available, so we will just ignore the Id_implicit.
			  */
		     }
		 } else if (attr == Id_include) {
		     /* take care that it gets installed in case it is a
		      * generated input, and the tool doesn't use %computed.
		      */
		     PutAttrValue(obj->e_object, Id_include,
				  TrueExpr, GLOBAL_SCOPE);
		 }
	    } else {
		val = GetAttrValue(obj->e_object, attr);
		/* avoid is_equal() calls by singling out the value UnKnown */
		if (val == UnKnown) {
		    if (spec->e_right != UnKnown) {
			break;
		    }
		} else if (!is_equal(val, &spec->e_right)) {
		    DBUG_PRINT("cluster", ("object %s fails attribute %s",
					   SystemName(obj->e_object),
					   attr->id_text));
		    break;
		}
	    }
	}
	if (acons == NULL) {	/* all attributes checked */
	    DBUG_PRINT("cluster", ("object %s matches all attributes",
				SystemName(obj->e_object)));
	    if (delete != NULL) { 	/* not a computed input */
		Remove(obj_list, cons);
		Append(delete, obj);
		if (persistent_seen && del_persistent != NULL) {
		    Append(del_persistent, obj);
		}
	    }
	    if (find_all) {
		Append(&found, obj);
	    } else {
		DBUG_RETURN(obj);
	    }
	}
    }
    if (find_all && !IsEmpty(found)) {
	struct expr *tmp = empty_list();

	tmp->e_list = found;
	tmp->e_type = T_LIST_OF | T_OBJECT;
	DBUG_RETURN(tmp);
    } else {
	DBUG_PRINT("cluster", ("no objects match all attributes"));
	DBUG_RETURN((struct expr *)NULL);
    }
}

PRIVATE struct expr *
DefaultComputedInputs(cluster, tool, param)
struct cluster *cluster;
struct tool *tool;
struct param *param;
{
    struct expr *binding, *files;
    int scope;

    /* mkdep-like programs need some special handling: they *compute*
     * their inputs. So, in order not to let mkdep run to early when
     * we are handling a cluster, we have to assume it uses maximum of
     * the matching files as input.
     * If this program has been run before, we will have some cooperation
     * from the cache, in that it will only check the actual inputs found
     * in the previous run.
     */

    /* first look if they have been found already */
    ITERATE(cluster->cl_def_inputs, expr, arrow, {
	assert(arrow->e_number == ARROW);
	if (arrow->e_left->e_idp == tool->tl_idp) {
	    return(CopyExpr(arrow->e_right));
	}
    });

    DBUG_PRINT("cluster", ("find default inputs for `%s'",
			   tool->tl_idp->id_text));
    files = FindFiles(cluster, T_LIST_OF|T_IN,
		      param->par_type->tp_attributes, (struct slist **)NULL,
		      (struct slist **)NULL, (struct slist **)NULL);
    if (files == NULL) {
	(files = empty_list())->e_type = T_LIST_OF | T_OBJECT;
    }

    scope = CurrentCluster->cl_scope->sc_number;
    ITERATE(files->e_list, expr, obj, {
	DBUG_PRINT("incl", ("found include file `%s'",
			    SystemName(obj->e_object)));
	PutAttrValue(obj->e_object, Id_include, TrueExpr, scope);
	PutAttrValue(obj->e_object, Id_include, TrueExpr, GLOBAL_SCOPE);
    });

    binding = new_expr(ARROW);
    binding->e_left = new_expr(ID);
    binding->e_left->e_idp = tool->tl_idp;
    binding->e_right = files;
    Append(&cluster->cl_def_inputs, binding);

    return(CopyExpr(files));
}

PUBLIC void
EvalDefault(call, arrow)
struct expr *call;
struct expr *arrow; /* where to put the result */
{
    struct param *param = arrow->e_left->e_param;
    struct expr **where = &arrow->e_right;
    
    if ((param->par_flags & PAR_COMPUTED) != 0) {
	if (CurrentCluster != NULL) {
	    *where = DefaultComputedInputs(CurrentCluster,
					   call->e_func->id_tool, param);
	} else {
	    DBUG_PRINT("cluster",("imperative command, computed inputs =>{}"));
	    (*where = empty_list())->e_type = T_LIST_OF | T_OBJECT;
	}
	return;
    }

    PutParams(call);
    /* Find a way to avoid multiple calls of PutParams.
     * Better yet: introduce an `environment' construction,
     * which is more clean than the current implementation.
     */

    CurrentParameter = param; /* `match_fn' uses this "hidden" parameter */
    if (*where == NULL) {
	*where = CopyExpr(param->par_default);
    } else {
        DBUG_PRINT("cluster", ("re-evaluating parameter `%s'",
			    param->par_idp->id_text));
    }
    *where = EvalDiagAlso(*where);
    *where = GetExprOfType(*where, param->par_type->tp_indicator & T_TYPE);

    /* if the default is a shared variable, put it on the list,
     * if not already done so.
     */
    if (IsShared(*where)) {
	SharedExpressionUsed(*where);
    }

    DBUG_EXECUTE("cluster", {
       DBUG_WRITE(("default of `%s' evaluates to: ", param->par_idp->id_text));
       DBUG_Expr(*where); DBUG_NL();
    });
}

#define HIGH 9999

PRIVATE int
SeqNr(obj, scope)
struct expr *obj;
int scope;
{
    register struct expr *seq_nr;

    if ((seq_nr = GetAttrInScope(obj->e_object, Id_seq_nr, scope))
	!= UnKnown) {
	DBUG_PRINT("seq", ("`%s' has seq_nr %ld", SystemName(obj->e_object),
			   seq_nr->e_integer));
	return(seq_nr->e_integer);
    } else {
	DBUG_PRINT("seq", ("`%s' has no seq_nr", SystemName(obj->e_object)));
	return(HIGH);
    }
}

PRIVATE struct cons *
InsertRising(listp, start, scope)
struct slist **listp;
register struct cons *start;
int scope;
/* Insert rising sequence starting at `start' in the (rising) head of th
 * list. Return the `cons' following the inserted rising sequence.
 */
{
    register int rising, cons_seq, sofar_seq, limit_seq;
    register struct cons *cons, *sofar;

    /* merge the rising sequence following `start' (inclusive) in the part
     * before `start'.
     */
    sofar = HeadOf(*listp);
    sofar_seq = SeqNr(ItemP(sofar, expr), scope);

    rising = 0;
    cons = start;

    limit_seq = SeqNr(ItemP(Prev(start), expr), scope);

    while (cons != NULL &&
	   (cons_seq = SeqNr(ItemP(cons, expr), scope)) <= limit_seq &&
	   cons_seq >= rising) {
	register struct expr *obj;
	register struct cons *next_one;

	/* find place to insert this one, start looking from `sofar' */
	while (sofar_seq < cons_seq) {
	    DBUG_PRINT("rise", ("skip `%s' (%d)",
				SystemName(ItemP(sofar, expr)->e_object),
				sofar_seq));
	    sofar = Next(sofar);
	    sofar_seq = SeqNr(ItemP(sofar, expr), scope);
	}

	DBUG_PRINT("rise", ("insert `%s' (%d) before `%s' (%d)",
			    SystemName(ItemP(cons, expr)->e_object),
			    cons_seq,
			    SystemName(ItemP(sofar, expr)->e_object),
			    sofar_seq));
			    
	/* do the move */
	next_one = Next(cons);
	obj = ItemP(cons, expr);
	Remove(listp, cons);
	Insert(listp, sofar, (generic) obj);
	cons = next_one;

	rising = cons_seq;
    }
    DBUG_PRINT("rise", ("done"));
    return(cons);
}

PRIVATE int
SortObjects(objects, scope)
struct expr **objects;
int scope;
{
    register int seq_nr;
    
    /* no sort yet, only look for lowest */
    if (((*objects)->e_type & T_LIST_OF) != 0) {
	/*
	 * DON'T pick a sort that is quick in general (e.g. Quicksort), but
	 * one that is suited for our situation: most of the time all we have
	 * to do is to move the tail of the objectlist in front.
	 */
	register int rising = 0;
	register struct cons *cons;

	for (cons = Head((*objects)->e_list); cons != NULL; ) {
	    register struct expr *obj = ItemP(cons, expr);

	    seq_nr = SeqNr(obj, scope);

	    if (seq_nr < rising) {
		/* insert all objects with rising sequence number, starting
		 * at cons, at an appropriate place in the object list.
		 */
		DBUG_PRINT("rise", ("`%s' (%d) is not in sequence",
				    SystemName(ItemP(cons, expr)->e_object),
				    seq_nr));
		cons = InsertRising(&(*objects)->e_list, cons, scope);
	    } else {
		rising = seq_nr;
		cons = Next(cons);
	    }
	}
	/* as the list is sorted now, return the seq_nr of the first one */
	if (IsEmpty((*objects)->e_list)) {
	    return(HIGH);
	} else {
	    return(SeqNr(ItemP(HeadOf((*objects)->e_list), expr), scope));
	}
    } else {
	assert((*objects)->e_number == OBJECT);
	return(SeqNr(*objects, scope));
    }
}

PRIVATE void
AddSeqNrs(objects, seq_nr, scope)
struct expr *objects;
int seq_nr, scope;
{
    if ((objects->e_type & T_LIST_OF) != 0) {
	ITERATE(objects->e_list, expr, obj, {
	    PutAttrValue(obj->e_object, Id_seq_nr,
			 IntExpr((long)seq_nr), scope);
	});
    } else {
	assert(objects->e_number == OBJECT);
	PutAttrValue(objects->e_object, Id_seq_nr,
		     IntExpr((long)seq_nr), scope);
    }
}

PUBLIC void
SolveSequenceProblems(inv, scope)
struct invocation *inv;
int scope;
/* Sort the objects in obj_list according to their seq_nr attribute.
 * Returns the lowest seq_nr found, or HIGH if not available.
 */
{
    register int lowest = HIGH, low;

    /* First find the input object with lowest seqence number, while
     * sorting the inputs.
     */
    ITERATE(inv->inv_call->e_args, expr, arrow, {
	register int type = arrow->e_left->e_param->par_type->tp_indicator;

	if (type & T_IN) {
	    DBUG_EXECUTE("cluster", {
		DBUG_WRITE(("UNSORTED")); DBUG_Expr(arrow->e_right); DBUG_NL();
	    });
	    if ((low = SortObjects(&arrow->e_right, scope)) < lowest) {
		lowest = low;
	    }
	    DBUG_EXECUTE("cluster", {
		DBUG_WRITE(("SORTED")); DBUG_Expr(arrow->e_right); DBUG_NL();
	    });
	}
    });

    /* Add this lowest sequence number to the output objects, so that
     * they can be kept in order when an other tool is invoked.
     */
    ITERATE(inv->inv_call->e_args, expr, arrow, {
	register int type = arrow->e_left->e_param->par_type->tp_indicator;

	if (type & T_OUT) {
	    AddSeqNrs(arrow->e_right, lowest, scope);
	}
    });

}

PRIVATE void
PutOutputAttributes(call)
struct expr *call;
{
    ITERATE(call->e_args, expr, arg, {
	register struct param *par = arg->e_left->e_param;

	if ((par->par_type->tp_indicator & T_OUT) != 0) {
	    register struct expr *attrlist = par->par_type->tp_attributes;

	    if (attrlist != NULL) {
		AddAttrList(arg->e_right, attrlist->e_list);
	    }
	}
    });
}

PRIVATE struct expr *
ConfArrow(call, conf)
struct expr *call;
struct idf *conf;
{
    ITERATE(call->e_args, expr, arrow, {
	if(arrow->e_left->e_param->par_idp == conf) {
	    return(arrow);
	}
    });
    return(NULL);
}

PRIVATE void
SetAbsentOK(obj_expr)
struct expr *obj_expr;
{
    /* Set flag that indicate that these objects will not
     * be produced (so that we may complain in the other case).
     * Note: this ought not be done with a global flag,
     * but I see no easy alternative, currently.
     */
    if (IsList(obj_expr)) {
	ITERATE(obj_expr->e_list, expr, obj, {
	    obj->e_object->ob_flags |= O_ABSENT_OK;
	});
    } else {
	obj_expr->e_object->ob_flags |= O_ABSENT_OK;
    }
}

PRIVATE struct expr *
TryTool(call, clus, inputs, outputs, add)
struct expr *call;
struct cluster *clus;
struct slist **inputs, **outputs, **add;
/* Bug: when this function finds that `call' cannot be completed, after a
 * an object-default has been evaluated, these defaults will be erroneously
 * be added to the cluster sources/targets in the assumption that they were
 * previously removed by FindFiles.
 * Requires rewrite/split-up.
 */
{
    /* Examine the tool header, and try to put appropriate objects from
     * the source and target list in the IN [LIST] cq. OUT [LIST] entries.
     * Otherwise, a default must be available or the tool doesn't apply.
     */
    register struct cons *cons;
    int hidden = FALSE;

    DBUG_ENTER("TryTool");
    for (cons = Head(call->e_args); cons != NULL; cons = Next(cons)) {
	register struct expr *obj, *arrow = ItemP(cons, expr);
	register struct param *param;
	int list_in_out, do_conform;

	param = arrow->e_left->e_param;
	list_in_out = param->par_type->tp_indicator & (T_IN|T_OUT|T_LIST_OF);

	do_conform = (param->par_flags & PAR_CONFORM) != 0;

	if (arrow->e_right == NULL) {   /* arg not assigned to */
	    if ((list_in_out & (T_IN|T_OUT)) != 0 &&
		(obj = FindFiles(clus, list_in_out,
				 param->par_type->tp_attributes,
				 inputs, outputs, &clus->cl_persistent))
		 != NULL) {
		DBUG_PRINT("cluster", ("Objects found:"));
		DBUG_EXECUTE("cluster", {
		    if (IsList(obj)) {
			ObjectsOnError(obj->e_list); NewLine();
		    } else {
			DBUG_Expr(obj); DBUG_NL();
		    }
		});
		arrow->e_right = obj;
		if (do_conform) {
		    /* conformance objects are needed, so set conformance
		     * parameter referred to accordingly.
		     */
		    ConfArrow(call, param->par_conform)->e_right = TrueExpr;
		}
		continue;
	    } else if (param->par_default == NULL &&
		       (param->par_flags & PAR_COMPUTED) == 0) {
		/* No object fits & no default available & no computed inputs.
		 * The tool cannot be applied, and we have to undo the changes
		 * made sofar. Bug still present: sometimes too much is
		 * `undone'.
		 */
		Verbose(1, "parameter `%s' couldn't be filled (anymore)",
			param->par_idp->id_text);
		AppendList(&clus->cl_sources->e_list, *inputs);
		AppendList(&clus->cl_targets->e_list, *outputs);

		put_expr_list(*inputs);
		put_expr_list(*outputs);
		put_expr_list(*add);
		*add = *inputs = *outputs = NULL;

		put_expr(call);
		DBUG_RETURN((struct expr *)NULL);
	    } /* else fallthrough */
	}

	/* evaluate default without suspension */
	DoSingleTask();
	EvalDefault(call, arrow);
	DoMultiTask();

	if (do_conform &&
	    IsFalse(ConfArrow(call, param->par_conform)->e_right)) {
	    SetAbsentOK(arrow->e_right);
	    continue; /* don't add as in/output */
	}

	if ((list_in_out & T_OUT) != 0) {
	    /* add default outputs to the cluster sources */
	    if ((list_in_out & T_LIST_OF) == 0) {
		if ((arrow->e_right->e_object->ob_flags & O_DIAG) == 0) {
		    /* don't add diagnostic files to the sources */
		    Append(add, arrow->e_right);
		}
	    } else {
		AppendList(add, arrow->e_right->e_list);
	    }
	} else if ((list_in_out & T_IN) != 0) {
	    DBUG_PRINT("cluster", ("hidden sources added"));
	    if ((list_in_out & T_LIST_OF) == 0) {
		Append(inputs, arrow->e_right);
	    } else {
		AppendList(inputs, arrow->e_right->e_list);
	    }
	    hidden = TRUE; /* hidden inputs added */
	}
    }
    /* Succes: whole call could be filled in. */
    AppendList(&clus->cl_sources->e_list, *add);
    AppendList(outputs, *add);

    put_expr_list(*add); *add = NULL;

    /* Now, check if we have changed anything in the clusters */
    if (!hidden && IsEmpty(*inputs) && IsEmpty(*outputs)) {
	Verbose(1, "tool doesn't change cluster");
	put_expr(call);
	DBUG_RETURN((struct expr *)NULL);
    } else {
	DBUG_EXECUTE("cluster", {
	    DBUG_WRITE(("sources now: ")); DBUG_Expr(clus->cl_sources); DBUG_NL();
	    DBUG_WRITE(("targets now: ")); DBUG_Expr(clus->cl_targets); DBUG_NL();
	});
	PutOutputAttributes(call);
	DBUG_RETURN(call);
    }
}


#define noregister static /* longjmp restores registers */
/* ANSI C has 'volatile' */

PUBLIC void
DetermineRelevantAttrValues(objects, attr_list)
struct slist *objects;
struct slist *attr_list;
{
    DBUG_ENTER("DetermineRelevantAttrValues");
    DoSingleTask();
    ITERATE(objects, expr, obj_expr, {
	ITERATE(attr_list, idf, attr, {
	    (void)GetAttrValue(obj_expr->e_object, attr);
	});
    });
    DoMultiTask();
    DBUG_VOID_RETURN;
}

PRIVATE void
ApplyTools(clus)
struct cluster *clus;
{
    noregister struct expr *Xto_be_filled, *Xfilled;
    noregister struct slist *Xinputs, *Xoutputs, *Xadd;
    jmp_buf LowLevelDetail;

    DBUG_ENTER("ApplyTools");
    while (!IsEmpty(clus->cl_tool_list) && !IsEmpty(clus->cl_sources)) {
	struct tool *tool;

	Wait();			/* wait till all invocations of the tool
				 * of the previous iteration are finished. 
				 */

	Xto_be_filled = ItemP(HeadOf(clus->cl_tool_list), expr);
	tool = Xto_be_filled->e_func->id_tool;

	Verbose(1, "try applying tool `%s'", tool->tl_idp->id_text);

	DetermineRelevantAttrValues(clus->cl_sources->e_list,
				    tool->tl_in_attr);
	DetermineRelevantAttrValues(clus->cl_targets->e_list,
				    tool->tl_out_attr);

	if (DBUG_CATCH(LowLevelDetail) == 0) {
	    Catch((jmp_buf *)LowLevelDetail);
	} else {
	    Verbose(3, "will be applied");
	    DBUG_PRINT("cluster", ("caught a hanging invocation"));
	}
	for (;;) {
	    CheckInterrupt();	/* don't let user wait too long */

	    AddScope(tool->tl_scope);
	    Xfilled = CopyExpr(Xto_be_filled);
	    RemoveScope();

	    Xadd = Xinputs = Xoutputs = NULL;
	    AttributeScopeNr = clus->cl_scope->sc_number;
	    /* may have been reset by previous invocation */

	    if ((Xfilled = TryTool(Xfilled, clus, &Xinputs, &Xoutputs, &Xadd))
		!= NULL) {
		Append(&clus->cl_calls, Xfilled);
		Invoke(Xfilled, Xinputs, Xoutputs);
		if (!F_no_exec) Verbose(3, "command was cached");
	    } else {
		break;
	    }
	}
	DBUG_END_CATCH();
	EndCatch();

	/* persistent objects can be used in following tools */
	AppendList(&clus->cl_sources->e_list, clus->cl_persistent);
	put_expr_list(clus->cl_persistent); clus->cl_persistent = NULL;

	/* 'invocation' can't be applied anymore, remove it */
	(void)RemoveHead(&clus->cl_tool_list, expr);
	put_expr(Xto_be_filled);
    }
    /* all tools tried, let's hope nothing is left */
    DBUG_VOID_RETURN;
}


typedef int ((*predicate)( /* struct expr *arg */ ));

PRIVATE int
IsIncludeFile(obj)
struct expr *obj;
{
    assert(CurrentCluster != NULL);
    return(IsTrue(GetAttrInScope(obj->e_object, Id_include,
				 CurrentCluster->cl_scope->sc_number)));
}

PUBLIC int
IsSource(obj)
struct expr *obj;
{
    assert(obj->e_number == OBJECT);
    assert(CurrentCluster != NULL);

    if (IsIncludeFile(obj)) {
	return(FALSE);
    } else {
	return(IsTrue(GetAttrInScope(obj->e_object, Id_is_source,
				     CurrentCluster->cl_scope->sc_number)));
    }
}

PUBLIC int
IsTarget(obj)
struct expr *obj;
{
    struct expr *target_of;

    assert(obj->e_number == OBJECT);
    assert(CurrentCluster != NULL);

    if ((target_of = GetAttrValue(obj->e_object, Id_target)) != UnKnown) {
	ITERATE(target_of->e_list, expr, cluster_id, {
	    assert(cluster_id->e_number == ID);
	    if (cluster_id->e_idp == CurrentCluster->cl_idp) {
		return(TRUE);
	    }
	});
    }

    return(FALSE);
}

PRIVATE int
IsIntermediate(obj)
struct expr *obj;
{
    return(!IsIncludeFile(obj) && !IsSource(obj));
}

PRIVATE int
UnusedIncludeFile(obj)
struct expr *obj;
{
    return(IsIncludeFile(obj) &&
	   !IsTrue(GetAttrInScope(obj->e_object, Id_computed,
				  CurrentCluster->cl_scope->sc_number)));
}

PRIVATE struct expr *
Select(list, pred)
struct slist *list;
predicate pred;
/* this function may be of use at other places, may even as user function? */
{
    struct expr *result = empty_list();

    ITERATE(list, expr, elem, {
	if (pred(elem)) {
	    Append(&result->e_list, elem);
	}
    });
    if (!IsEmpty(result->e_list)) {
	result->e_type =
	    T_LIST_OF | ItemP(HeadOf(result->e_list), expr)->e_type;
    }
    return(result);
}

PRIVATE void
CheckCleanCluster(clus)
struct cluster *clus;
{
    register struct cons *cons, *next;

    /* First remove the sources that have been used, but who are still
     * present in the sources. This is the case with implicit inputs like
     * header files.
     */
    for (cons = Head(clus->cl_sources->e_list); cons != NULL;   ) {
	register struct expr *obj_expr = ItemP(cons, expr);

	next = Next(cons);
	if (GetAttrInScope(obj_expr->e_object, Id_computed,
			   clus->cl_scope->sc_number) == TrueExpr) {
	    Remove(&clus->cl_sources->e_list, cons);
	}
	cons = next;
    }

    if (!IsEmpty(clus->cl_sources->e_list)) {
	struct expr *selection;

	selection = Select(clus->cl_sources->e_list, IsSource);
	if (!IsEmpty(selection->e_list)) {
	    PrintCluster(clus);
	    PrintError(" didn't use the following sources:\n");
	    ObjectsOnError(selection->e_list); NewLine();
        }
	put_expr(selection);

	if (F_verbose && !F_no_exec) {
	    selection = Select(clus->cl_sources->e_list, UnusedIncludeFile);
	    if (!IsEmpty(selection->e_list)) {
		PrintCluster(clus);
		PrintError(" didn't use the following include files:\n");
		ObjectsOnError(selection->e_list); NewLine();
	    }
	    put_expr(selection);
	}

	selection = Select(clus->cl_sources->e_list, IsIntermediate);
	if (!IsEmpty(selection->e_list)) {
	    PrintCluster(clus);
	    PrintError(" didn't use the following produced objects:\n");
	    ObjectsOnError(selection->e_list); NewLine();
        }
	put_expr(selection);
    }
    if (!IsEmpty(clus->cl_targets->e_list)) {
	PrintCluster(clus);
	PrintError(" can't produce the following targets:\n");
	ObjectsOnError(clus->cl_targets->e_list); NewLine();
	DBUG_EXECUTE("failure", {
	    ITERATE(clus->cl_targets->e_list, expr, obj, {
		PrintObject(obj->e_object);
	    });
	});
    }
}

PRIVATE  void
TargetsFailed(clus)
struct cluster *clus;
{
    extern void SetBadExitStatus();

    /* Iterate over ALL original targets, the cluster algorithm has possibly
     * removed them all from cl_targets!
     */
    ITERATE(clus->cl_orig_targets->e_list, expr, obj_expr, {
        Taint(obj_expr->e_object);
    });
    SetBadExitStatus();
}

PRIVATE void
MakeAvailable(targets)
struct slist *targets;
{
    ITERATE(targets, expr, target, {
	PutAttrValue(target->e_object, Id_target, UnKnown, GLOBAL_SCOPE);
	PutAttrValue(target->e_object, Id_installed, TrueExpr, GLOBAL_SCOPE);
    });
}

PRIVATE void
CheckResults(clus)
struct cluster *clus;
{
    ITERATE(clus->cl_calls, expr, res, {
	if (IsFalse(res)) {
	    PrintCluster(clus);
	    PrintError(" didn't finish successfully\n");
	    /* taint the targets, they might be used by other clusters */
	    TargetsFailed(clus);
	    break;
	}
    });
    /* original targets again! */
    MakeAvailable(clus->cl_orig_targets->e_list);
}

PRIVATE struct expr *
FillIn(inv, use)
struct expr *inv, *use;
{
    ITERATE(inv->e_args, expr, arrow, {
	register struct cons *acons;

	for (acons = Head(use->e_args); acons != NULL; acons = Next(acons)) {
	    register struct expr *use_arrow = ItemP(acons, expr);

	    if (use_arrow->e_left->e_param->par_idp ==
		arrow->e_left->e_param->par_idp) {
		arrow->e_right = use_arrow->e_right; /* no copy/eval? */
		break;
	    }
	}
    });
}

PRIVATE void
DoViewPathing(clus)
struct cluster *clus;
/* Take care of viewpathing, by trying to derive the `dir' attribute of
 * all sources. Then go through the sources, replacing the ones that,
 * according to their `dir' attribute, should get another parent.
 * The attributes present on the previous object are merged on the new one.
 */
{
    register struct expr *dir, *new;

    DetermineRelevantAttrValues(clus->cl_sources->e_list, DirAttribute);
    Wait(); /* wait, if necessary, till they are available */
    ITERATE(clus->cl_sources->e_list, expr, obj_expr, {
	dir = GetAttrValue(obj_expr->e_object, Id_dir);
	if (dir->e_type == T_OBJECT &&
	    dir->e_object != obj_expr->e_object->ob_parent) {
	    new = ObjectExpr(ObjectWithinParent(obj_expr->e_object->ob_idp,
						dir->e_object));
	    DBUG_PRINT("merge", ("viewpathing delivers `%s'",
				 SystemName(new->e_object)));
	    MergeAttributes(obj_expr->e_object, new->e_object);
	    Item(cons) = (generic) new; /* replace source */
	}
    });
}

#define MAX_TOOL_PRIO 999	/* surely not more than MAX_TOOL_PRIO tools */

PRIVATE void
PutToolsInGraph(clus, tools)
struct cluster *clus;
struct slist *tools;
{

    ITERATE(tools, tool, t, {
	int use_prio; /* earliest tools in %use clause have highest priority*/
	register struct cons *tcons;
	struct expr *inv = MakeCallExpr(t->tl_idp);

	/* look if this tool has a %use clause defined for it */
	for (tcons = Head(clus->cl_invocations), use_prio = MAX_TOOL_PRIO;
	     tcons != NULL;
	     tcons = Next(tcons), --use_prio) {
	    struct expr *use = ItemP(tcons, expr);
	    
	    if (use->e_func == t->tl_idp) {
		FillIn(inv, use);
		PutInvocationInGraph(clus->cl_graph, inv, use_prio);
		break;
	    }
	}
	if (tcons == NULL) { /* no %use clause */
	    PutInvocationInGraph(clus->cl_graph, inv, 0);
	}

	DBUG_EXECUTE("tool", {
	    DBUG_WRITE(("tool `%s' for `%s': ",
			t->tl_idp->id_text, clus->cl_idp->id_text));
	    DBUG_Expr(inv); DBUG_NL();
	});
    });
}

PUBLIC void
ClusterAlgorithm(clusp)
struct cluster **clusp;
{
    struct cluster *clus = *clusp;

    DBUG_ENTER("ClusterAlgoritm"); 
    CurrentCluster = clus;
    AttributeScopeNr = clus->cl_scope->sc_number;

    ResetScopes();
    AddScope(clus->cl_scope);

    if (clus->cl_state != CL_SRC) {
	if (F_verbose && Verbosity >= 2) {
	    PrintCluster(clus);
	    PrintError(" running\n");
	}
    }

    switch (clus->cl_state) {
    case CL_SRC:
	if (clus->cl_ident != NULL) { /* name not yet expanded */
	    struct idf *new_id;

	    clus->cl_ident = GetExprOfType(Eval(clus->cl_ident), T_STRING);
	    new_id = get_id(clus->cl_ident);

	    DeclareIdent(&new_id, I_CLUSTER);
	    /* creates a new cluster, but we already have one, so remove it */
	    free_cluster(new_id->id_cluster);
	    new_id->id_cluster = clus;
	    clus->cl_idp = new_id;
	    /* put_expr(clus->cl_ident); */ clus->cl_ident = NULL;
	}

	DBUG_PRINT("cluster", ("computing sources"));
	clus->cl_sources = GetExprOfType(Eval(clus->cl_sources),
					 T_LIST_OF | T_OBJECT);
	/* The old algorithm, which is (still) used to figure out
	 * what invocations need to be made, changes the source and
	 * target list of the cluster. So if it is shared (possibly with
	 * another cluster), copy it first 
	 */
	clus->cl_sources = CopySharedExpr(clus->cl_sources);
	DoViewPathing(clus);
	
	DBUG_PRINT("cluster",("src of cluster `%s': ", clus->cl_idp->id_text));
	DBUG_EXECUTE("cluster", {
	    ObjectsOnError(clus->cl_sources->e_list); NewLine();
	});

	clus->cl_state = CL_TAR;
	/* fall through */
    case CL_TAR:
	DBUG_PRINT("cluster", ("computing targets"));
	clus->cl_targets = GetExprOfType(Eval(clus->cl_targets),
					 T_LIST_OF | T_OBJECT);
	clus->cl_orig_targets = CopyExpr(clus->cl_targets);
	/* target list is changed, so: */
	clus->cl_targets = CopySharedExpr(clus->cl_targets);

	DBUG_PRINT("cluster", ("targets of cluster `%s': ",
			       clus->cl_idp->id_text));
	DBUG_EXECUTE("cluster", {
	    ObjectsOnError(clus->cl_targets->e_list); NewLine();
	});

	if (F_cleanup) {
	    ITERATE(clus->cl_targets->e_list, expr, obj, {
		DoDelete(SystemName(obj->e_object), NON_FATAL);
	    });
	    /* when cleaning up, don't run the normal algorithm */
	    break;
	}

	MarkTargets(clus);
	clus->cl_state = CL_SYN;

    case CL_SYN:
	DBUG_PRINT("cluster", ("synchronising"));
	clus->cl_state = CL_CHK; 		/* only do one Synch */
	Synchronisation(clus);
    case CL_CHK:
	if ((clus->cl_flags & C_DO_RUN) == 0) {
	    DBUG_PRINT("cluster", ("cluster `%s' will not be run",
				   clus->cl_idp->id_text));
	    break;
	}
	MarkSources(clus);
	clus->cl_state = CL_CMD;
    case CL_CMD:
	if ((clus->cl_flags & C_IMPERATIVE) != 0) {
	    AllocInputs(clus->cl_sources->e_list);
	    /* targets don't have to be allocated, because they
	     * won't generate conflicts.
	     */
	    Wait();
	    if (!MarkInputs(clus->cl_sources->e_list, (struct job *)NULL)) {
		TargetsFailed(clus);
	    } else {
		clus->cl_command = Eval(clus->cl_command);
		if (IsFalse(clus->cl_command)) {
		    Warning("%%do clause for cluster `%s' failed",
			    clus->cl_idp->id_text);
		    TargetsFailed(clus);
		}
	    }
	    MakeAvailable(clus->cl_targets->e_list);
	    /* only needed for objects created with direct exec-call */
	    break;
	} else {
	    InitGraph(&clus->cl_graph);
	    PutToolsInGraph(clus, ToolList);
	    PutObjectsInGraph(clus->cl_graph, clus->cl_sources->e_list);
	    PutObjectsInGraph(clus->cl_graph, clus->cl_targets->e_list);
	    if (PruneGraph(clus->cl_graph, clus)) {
		DBUG_PRINT("class", ("graph after pruning"));
		PrintGraph(clus->cl_graph);
		/* Change the tool order so that we can safely iterate
		 * over the list of tools. This takes care, e.g., that
		 * "yacc" will be considered before "cc-c", even if
		 * yacc is defined as last tool.
		 */
		ChangeToolOrder(clus);
	    } else {
		PrintCluster(clus);
		PrintError(" has an inconsistent tool graph\n");
		TargetsFailed(clus);
		MakeAvailable(clus->cl_targets->e_list);
		break;
	    }
	    clus->cl_state = CL_COMPUTE;
	}
    case CL_COMPUTE:
	ApplyTools(clus);
	Wait();				/* wait for all invocations */
	CheckCleanCluster(clus);	/* No (real) sources or targets left */
	clus->cl_state = CL_WAIT;
    case CL_WAIT:
	DBUG_PRINT("cluster", ("wait for commands"));
	Wait();
	CheckResults(clus);
	break;
    default:
	CaseError("ClusterAlgorithm: %d", (int) clus->cl_state);
    }
    CurrentCluster = NULL;

    RemoveScope();
    DBUG_VOID_RETURN;
}


PRIVATE struct slist *
ToolHeaders(clus, tools)
struct cluster *clus;
struct slist *tools;
{
    struct slist *headers = NULL;

    /* ## will be obsolete soon ## */

    /* idea: make a toolheader only once, when it is defined.
     * This is put in the tool structure.
     * When one is needed, CopyExpr can be used to create a new one.
     */

    ITERATE(tools, tool, t, {
	register struct cons *tcons;
	struct expr *inv = MakeCallExpr(t->tl_idp);

	/* look if this tool has a use-clause defined for it */
	for (tcons = Head(clus->cl_invocations); tcons != NULL;
	     tcons = Next(tcons)) {
	    struct expr *use = ItemP(tcons, expr);

	    if (use->e_func == t->tl_idp) {
		FillIn(inv, use);
		break;
	    }
	}
	DBUG_EXECUTE("cluster", {
	    DBUG_WRITE(("tool `%s' for `%s': ",
			t->tl_idp->id_text, clus->cl_idp->id_text));
	    DBUG_Expr(inv); DBUG_NL();
	});
	Append(&headers, inv);
    });
    return(headers);
}


PUBLIC void
EnterCluster(clus)
struct cluster *clus;
{
    DBUG_ENTER("EnterCluster");
    clus->cl_job = CreateJob(ClassCluster, (generic)clus);
    clus->cl_state = CL_SRC;	/* start with computing the sources */

    if ((clus->cl_flags & C_IMPERATIVE) == 0) {
	/* Add the list of tools to be used. For the moment: simply make a
         * copy of the header of all the tools known until now (or should it
	 * be done when all input has been read?).
	 * The cl_invocations list also has to be taken in account, but they
	 * are ignored for the moment.
	 */
	clus->cl_tool_list = ToolHeaders(clus, ToolList);
    }

    NrClustersRead++;
    Append(&AllClusters, clus);
    DBUG_VOID_RETURN;
}

/*ARGSUSED*/
PRIVATE void
RmCluster(clus)
struct cluster *clus;
{
    /* We could throw away the cluster and the lists it contains here. */
    DBUG_PRINT("job", ("tried to throw away cluster"));
}

PRIVATE char *
PrCluster(clus)
struct cluster *clus;
{
    if ((clus->cl_flags & C_ANONYMOUS) != 0 &&
	((int)clus->cl_state) > ((int)CL_TAR) &&
	!IsEmpty(clus->cl_orig_targets->e_list)) {
	(void)sprintf(ContextString, "cluster producing `%s'",
		     SystemName(ItemP(HeadOf(clus->cl_orig_targets->e_list),
				      expr)->e_object));
    } else {
	(void)sprintf(ContextString, "cluster `%s'", clus->cl_idp->id_text);
    }
    return(ContextString);
}

PUBLIC void
InitClusters(to_do)
struct slist *to_do;
{
    ClustersOrObjectsToDo = to_do;
    ClassCluster = NewJobClass("cluster", (void_func) ClusterAlgorithm,
			       (void_func) RmCluster, (string_func) PrCluster);
    DeclareAttribute(Id_persistent = str2idf("persistent", 0));
    Id_persistent->id_flags |= F_SPECIAL_ATTR;

    Append(&DirAttribute, Id_dir);
}

