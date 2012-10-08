/*	@(#)classes.c	1.2	94/04/07 14:47:37 */
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
#include "error.h"
#include "slist.h"
#include "expr.h"
#include "dbug.h"
#include "type.h"
#include "eval.h"
#include "tools.h"
#include "scope.h"
#include "Lpars.h"
#include "objects.h"
#include "cluster.h"
#include "caching.h" /* Id_compare */
#include "dump.h"
#include "classes.h"

typedef struct slist *tool_list;
typedef struct slist *arrow_list;
typedef struct slist *object_list;


/*
 * A cluster may redefine options for the tools it uses.
 * These are (going to be) represented in a tool-instance structure.
 * The toolinstance structure is also, via its parameterlist, linked to
 * the in- and output attribute classes, which, during execution of the
 * new cluster algorithm, contain objects to be used.
 */

#define TI_MARKED 	0x01
#define TI_BEEN_HERE	0x02

struct toolinstance {
    struct tool *ti_tool;	  /* containing the (global) definition */
    int 	 ti_flags;
    int		 ti_index;	  /* the order of tools referred to in %use
				   * clauses can be used to resolve some
				   * conflicts
				   */
    struct expr *ti_header;	  /* possibly alternative defaults */
    struct expr *ti_invocations;  /* running instantiations of this tool,
				   * currently they are put in the cluster
				   * itself
				   */
    struct toolinstance *ti_next; /* next one in the ordering found */
};

#include "structdef.h"
/* allocation definitions of struct toolinstance */
DEF_STRUCT(struct toolinstance, h_toolinstance, cnt_toolinstance)
#define new_toolinstance()	NEW_STRUCT(struct toolinstance, h_toolinstance, cnt_toolinstance, 5)
#define free_toolinstance(p)	FREE_STRUCT(struct toolinstance, h_toolinstance, p)

/* allocation definitions of struct graph */
DEF_STRUCT(struct graph, h_graph, cnt_graph)
#define new_graph()		NEW_STRUCT(struct graph, h_graph, cnt_graph, 5)
#define free_graph(p)		FREE_STRUCT(struct graph, h_graph, p)


PRIVATE struct graph *GlobalGraph;
/* each cluster gets its own copy, instantiated with objects, tools, etc. */

/*
 * An `objclass' in represents a set of (attribute, value) bindings. A class
 * may contain links to toolinstance it is input/output of. It also
 * has a list objects belonging to it.
 *
 * Datastructure:
 * cs_attr_value: a list of (attribute => value) pairs; an arrow node is
 * just an expr node with e->e_left an ID-expr and e->e_right an arbitrary
 * expr. This could be done more efficient: the e->e_left can be replaced
 * by an e->e_id, as is done with '(' nodes. For the moment, just keep the
 * old representation.
 */

struct objclass {
    arrow_list    cs_attr_values;   /* identifying this class uniquely */
    int		  cs_flags;	    /* recursive visiting etc. */
    class_list    cs_subs;	    /* its subclasses */
    class_list    cs_supers;        /* its superclasses */
    object_list   cs_objects;	    /* objects in this class */
    tool_list     cs_input_tools;   /* tools that have this class as input */
    tool_list     cs_persistent_tools; /* tools having it as persistent input*/
    tool_list     cs_output_tools;  /* tools that have this class as output */
    struct objclass *cs_copy;	    /* used when copying a class structure */
};

/* allocation definitions of struct objclass */
DEF_STRUCT(struct objclass, h_objclass, cnt_objclass)
#define new_objclass()   NEW_STRUCT(struct objclass, h_objclass, cnt_objclass, 5)
#define free_objclass(p) FREE_STRUCT(struct objclass, h_objclass, p)

/* marks */
#define C_SUB		0x01	/* subclass */
#define C_SUPER		0x02	/* superclass */
#define C_NO_IMM_SUB	0x04	/* no immediate subclass */
#define C_NO_IMM_SUPER	0x08	/* no immediate superclass */
#define C_MARKS		(C_SUB|C_SUPER|C_NO_IMM_SUB|C_NO_IMM_SUPER)

/* flags */
#define C_INPUT		0x20	/* (recursive) input class */
#define C_OUTPUT	0x40	/* (recursive) output class */
#define C_MARKED_OUTPUT 0x80	/* (recursive) output class of marked tool */

PRIVATE void
FreeSlist(list)
struct slist **list;
{
    /* should be done (more efficiently) in slist module */
    register struct cons *cons, *next;

    for (cons = Head(*list); cons != NULL; ) {
	next = Next(cons);
	Remove(list, cons);
	cons = next;
    }
}

PRIVATE struct expr *
FindAttr(attr_val, attr)
arrow_list attr_val;
struct idf *attr;
/* return arrow node in attr_val having attribute attr, or NULL */
{
    ITERATE(attr_val, expr, arrow, {
	if (arrow->e_left->e_idp == attr) {
	    return(arrow);
	}
    });
    return(NULL);
}

PRIVATE int
IsSubclass(sub, super)
arrow_list sub, super;
/* Return TRUE iff 'sub' is a subclass of 'super'.
 * Algorithm: go through all attributes of 'super', and check if
 * they have the same value as has the ones in 'sub'.
 */
{
    register struct expr *sub_arrow;

    ITERATE(super, expr, arrow, {
	if ((sub_arrow = FindAttr(sub, arrow->e_left->e_idp)) == NULL) {
	    /* super contains an attribute not present on sub */
	    if ((arrow->e_left->e_idp->id_flags & F_SPECIAL_ATTR) != 0 ||
		arrow->e_right == UnKnown) {
		/* (non-)presence of special attributes doesn't concern us,
		 * yet (but watch out `persistent', that *does* have
		 * significance for the new algorithm!)
		 * If the value of this attribute is %unknown, just skip it
		 */
		continue;
	    } else {
		/* not %unknown, so super can't be sub's superclass */
		return(FALSE);
	    }
	} else if (!is_equal(sub_arrow->e_right, &arrow->e_right)) {
	    /* super has another value for some attribute, so it can't be
	     * sub's superclass
	     */
	    return(FALSE);
	}
    });
    /* all (attr, value) combinations of super are present on sub, so
     * sub is indeed a subclass.
     */
    return(TRUE);
}
     
enum order { SAMECLASS, SUBCLASS, SUPERCLASS, NO_ORDER };

PRIVATE enum order
Ordering(aval1, aval2)
arrow_list aval1, aval2;
/* determine the order between the classes aval1 and aval2 */
{
    register int sub12, sub21;

    sub12 = IsSubclass(aval1, aval2);
    sub21 = IsSubclass(aval2, aval1);

    if (sub12) {
	if (sub21) {
	    return(SAMECLASS);
	} else {
	    return(SUBCLASS);
	}
    } else {
	if (sub21) {
	    return(SUPERCLASS);
	} else {
	    return(NO_ORDER);
	}
    }    
}

#ifdef DEBUG
PRIVATE void
DumpAttrValues(alist)
arrow_list alist;
{
    DBUG_EXECUTE("class", {
	ITERATE(alist, expr, arrow, {
	    DBUG_WRITE(("[%s=", arrow->e_left->e_idp->id_text));
	    PrExpr(arrow->e_right);
	    DBUG_WRITE(("]"));
	});
    });
}
#endif

#ifdef DEBUG
#define PrintClass(c) DumpAttrValues(c->cs_attr_values)
#else
#define PrintClass(c)
#endif

#ifdef DEBUG

PRIVATE void
PrintClassInfo(c)
struct objclass *c;
{
    DBUG_EXECUTE("class", {
	PrintClass(c); DBUG_NL();
    });

    /* list for each of classes it's superclasses */
    DBUG_EXECUTE("super", {
	if (!IsEmpty(c->cs_supers)) {
	    DBUG_WRITE(("super: "));
	    ITERATE(c->cs_supers, objclass, super, {
		PrintClass(super); DBUG_WRITE(("; "));
	    });
	    DBUG_NL();
	}
    });

    /* same for subclasses */
    DBUG_EXECUTE("sub", {
	if (!IsEmpty(c->cs_subs)) {
	    DBUG_WRITE(("sub: "));
	    ITERATE(c->cs_subs, objclass, sub, {
		PrintClass(sub); DBUG_WRITE(("; "));
	    });
	    DBUG_NL();
	}
    });

    /* objects in this class, if any */
    DBUG_EXECUTE("objects", {
	if (!IsEmpty(c->cs_objects)) {
	    DBUG_WRITE(("objects: "));
	    ITERATE(c->cs_objects, expr, obj_expr, {
		DBUG_WRITE(("%s; ", SystemName(obj_expr->e_object)));
	    });
	    DBUG_NL();
	}
    });

    DBUG_EXECUTE("tools", {
	if (!IsEmpty(c->cs_input_tools)) {
	    DBUG_WRITE(("input of "));
	    ITERATE(c->cs_input_tools, toolinstance, ti, {
		DBUG_WRITE(("%s; ", ti->ti_tool->tl_idp->id_text));
	    });
	    DBUG_NL();
	}
    });

    DBUG_EXECUTE("tools", {
	if (!IsEmpty(c->cs_persistent_tools)) {
	    DBUG_WRITE(("persistent input of "));
	    ITERATE(c->cs_persistent_tools, toolinstance, ti, {
		DBUG_WRITE(("%s; ", ti->ti_tool->tl_idp->id_text));
	    });
	    DBUG_NL();
	}
    });

    DBUG_EXECUTE("tools", {
	if (!IsEmpty(c->cs_output_tools)) {
	    DBUG_WRITE(("output of "));
	    ITERATE(c->cs_output_tools, toolinstance, ti, {
		DBUG_WRITE(("%s; ", ti->ti_tool->tl_idp->id_text));
	    });
	    DBUG_NL();
	}
    });
}
#endif

PRIVATE void
RemoveLink(sub, super)
struct objclass *sub, *super;
/* remove the links between sub and super (which must exist) */
{
    int removed = 0;

    ITERATE(sub->cs_supers, objclass, c, {
	if (c == super) {
	    DBUG_EXECUTE("class", {
		DBUG_WRITE(("remove superclass ")); PrintClass(c);
		DBUG_WRITE(("from subclass")); PrintClass(sub); DBUG_NL();
	    });
	    Remove(&sub->cs_supers, cons);
	    removed++;
	    break;
	}
    });
    ITERATE(super->cs_subs, objclass, c, {
	if (c == sub) {
	    DBUG_EXECUTE("class", {
		DBUG_WRITE(("remove subclass ")); PrintClass(c);
		DBUG_WRITE(("from superclass")); PrintClass(super); DBUG_NL();
	    });
	    Remove(&super->cs_subs, cons);
	    removed++;
	    break;
	}
    });

    assert(removed == 2);
}

PRIVATE void
MarkSuper(c, flag)
struct objclass *c;
int flag;
/* Recursively mark superclasses of class c with flag. */
{
    ITERATE(c->cs_supers, objclass, super, {
	if ((super->cs_flags & flag) != flag) {
	    super->cs_flags |= flag;
	    MarkSuper(super, flag);
	} /* else already visited super */
    });
}

PRIVATE void
MarkSub(c, flag)
struct objclass *c;
int flag;
{
    ITERATE(c->cs_subs, objclass, sub, {
	if ((sub->cs_flags & flag) != flag) {
	    sub->cs_flags |= flag;
	    MarkSub(sub, flag);
	} /* else already visited sub */
    });
}

PRIVATE void
UnMark(classes)
class_list classes;
{
    ITERATE(classes, objclass, c, {
	c->cs_flags &= ~C_MARKS;
    });
}

PRIVATE arrow_list
AttrBindings(obj_expr, attrs)
struct expr *obj_expr;
struct slist *attrs;
{
    arrow_list bindings = NULL;

    /* create a list of attribute bindings mentioned in attrs */
    ITERATE(attrs, idf, idp, {
	register struct expr *val;

	if ((val = GetAttrValue(obj_expr->e_object, idp)) != UnKnown) {
	    register struct expr *new = new_expr('=');

	    new->e_right = CopyExpr(val);
	    new->e_left = new_expr(ID);
	    new->e_left->e_idp = idp;
	    Append(&bindings, new);
	}
    });
    return(bindings);
}

PRIVATE arrow_list
CopyAttrBindings(alist)
arrow_list alist;
{
    arrow_list bindings = NULL;

    ITERATE(alist, expr, arrow, {
	if ((arrow->e_left->e_idp->id_flags & F_SPECIAL_ATTR) == 0) {
	    /* skip special attributes used in tooldefs */
	    register struct expr *new = new_expr('=');

	    new->e_left = CopyExpr(arrow->e_left);
	    new->e_right = CopyExpr(arrow->e_right);
	    Append(&bindings, new);
	}
    });
    return(bindings);
}

PRIVATE struct objclass *
AddClass(class_listp, attr_val)
class_list *class_listp;
arrow_list attr_val;
/* Put the class described by 'attr_val' to class_listp, if it not already
 * exists. The non-redundant ordering of class_listp has to be preserved.
 */
{
    struct objclass *new;
    class_list	all_subs = NULL, all_supers = NULL;

    ITERATE(*class_listp, objclass, existing, {
	switch (Ordering(attr_val, existing->cs_attr_values)) {
	case SAMECLASS:	/* this class already exists */
			FreeSlist(&all_supers);
			FreeSlist(&all_subs);
			UnMark(*class_listp);
			return(existing);
	case SUBCLASS:	Append(&all_supers, existing);
			existing->cs_flags |= C_SUB;
			break;
	case SUPERCLASS:Append(&all_subs, existing);
			/* existing->cs_flags |= C_SUPER; */
			break;
	}
    });

    /* If we come here, a new class was presented.
     * We only have to make the links from the existing classes to the
     * new one.
     */
    new = new_objclass();
    new->cs_attr_values = CopyAttrBindings(attr_val);
    Append(class_listp, new);

    DBUG_EXECUTE("addclass", {
	DBUG_WRITE(("Class to be added:"));
	PrintClass(new);
	DBUG_NL();
    });

    /* recursively mark superclasses, so that only immediate superclasses
     * remain unmarked.
     */
    ITERATE(all_supers, objclass, super, {
	MarkSuper(super, C_NO_IMM_SUPER);
    });

    /* the same for all subclasses */
    ITERATE(all_subs, objclass, sub, {
	MarkSub(sub, C_NO_IMM_SUB);
    });

    /* Install links between new and all immediate superclasses */
    ITERATE(all_supers, objclass, super, {
	if ((super->cs_flags & C_NO_IMM_SUPER) == 0) {
	    Append(&super->cs_subs, new);
	    Append(&new->cs_supers, super);

	    /* also remove the links between super and any of the
             * immediate subclasses of new
	     */
	    ITERATE(super->cs_subs, objclass, c, {
		if ((c->cs_flags & C_SUB) != 0 &&
		    (c->cs_flags & C_NO_IMM_SUB) == 0) {
		    RemoveLink(c, super);
		}
	    });
	}
    });

    ITERATE(all_subs, objclass, sub, {
	if ((sub->cs_flags & C_NO_IMM_SUB) == 0) {
	    Append(&sub->cs_supers, new);
	    Append(&new->cs_subs, sub);
	}
    });

    FreeSlist(&all_subs);
    FreeSlist(&all_supers);
    UnMark(*class_listp);

    DBUG_EXECUTE("addclass", {
	DBUG_WRITE(("After adding:"));
	PrintClassInfo(new);
	DBUG_NL(); DBUG_NL();
    });

    return(new);
}


#ifdef DEBUG
PUBLIC void
DoPrintGraph(graph)
struct graph *graph;
{
    ITERATE(graph->gr_classes, objclass, c, {
	PrintClassInfo(c);
    });
}
#endif

/*
 * The in- & output attributes together are kept on a list.
 * These are the attributes according to which the objects are to be
 * divided among the attribute classes.
 */

PRIVATE struct slist
    *IO_attributes = NULL,
    *IO_special_attributes = NULL;

PRIVATE void
AddIOAttrs(attrs)
arrow_list attrs;
{
    ITERATE(attrs, expr, arrow, {
	register struct idf *idp = arrow->e_left->e_idp;

	if ((idp->id_flags & (F_IO_ATTR | F_SPECIAL_ATTR)) == 0) {
	    Append(&IO_attributes, arrow->e_left->e_idp);
	    Append(&IO_special_attributes, arrow->e_left->e_idp);
	    idp->id_flags |= F_IO_ATTR;
	}
    });
}


/*
 * While tools are being read, a global class/tool structure is built
 * up. When the cluster algorithm has to be run for a specific cluster,
 * a *copy* of this global structure (including new classes) is to
 * be created, and additional info (sources, targets, use-clauses) filled
 * in. This is needed because the algorithm is destructive: the graph
 * is `pruned' while the algorithm runs. Also, the actual tool ordering may
 * be different among several clusters.
 */

PRIVATE int
ContainsPersistent(alist)
arrow_list alist;
{
    ITERATE(alist, expr, spec, {
	if (spec->e_left->e_idp == Id_persistent) {
	    return(TRUE); /* what about [persistent = expression] ? */
	}
    });
    return(FALSE);
}

PUBLIC void
PutInvocationInGraph(graph, inv, index)
struct graph *graph;
struct expr *inv;
int index;
/* Add tool to global class structure, by means a `toolinstance' struct.
 * As each cluster gets a copy of the class structure, it may for itself
 * create alternative default bindings for its parameters, as specified
 * in the tool's use-clause.
 */
{
    register struct tool *tool = inv->e_func->id_tool;
    register struct toolinstance *ti;

    ti = new_toolinstance();
    ti->ti_tool = tool;
    ti->ti_header = inv;
    ti->ti_index = index;
    Append(&graph->gr_tools, ti);

    ITERATE(tool->tl_scope->sc_scope, param, par, {
	register struct type *t = par->par_type;
	register int inout = t->tp_indicator & (T_IN | T_OUT);
	
	if (inout != 0) {
	    struct objclass *c;

	    c = AddClass(&graph->gr_classes,
			 t->tp_attributes->e_list);
	    /* what about tools having the same class twice as I/O ? */

	    if (inout == T_IN) {
		if (ContainsPersistent(t->tp_attributes->e_list)) {
		    Append(&c->cs_persistent_tools, ti);
		} else {
		    Append(&c->cs_input_tools, ti);
		}
	    } else {
		Append(&c->cs_output_tools, ti);
	    }

	    par->par_class = c;
	    /* each tool in each cluster must have its own param nodes
	     * for this to work
	     */
	}
    });
}

PRIVATE struct slist *DeclaredTools = NULL;

PUBLIC void
DoAttributes(tool)
struct tool *tool;
{
    ITERATE(tool->tl_scope->sc_scope, param, par, {
	register struct type *t = par->par_type;
	register int inout = t->tp_indicator & (T_IN | T_OUT);
	
	if (inout != 0) {
	    if (IsEmpty(t->tp_attributes->e_list)) {
		/* cluster algorithm won't probably do what is intended */
		Warning("parameter `%s' of `%s' has an empty specification",
			par->par_idp->id_text, tool->tl_idp->id_text);
	    } else {
		AddIOAttrs(t->tp_attributes->e_list);
	    }
	}
    });
}

PUBLIC void
DeclareTool(tool)
struct tool *tool;
{
    DoAttributes(tool);
    Append(&DeclaredTools, tool);
}

/*
 * Sometimes an output-attribute specification isn't complete:
 * if a tool generates a [type = C-incl] file, we should derive that
 * in fact [type = C-incl, C-incl] was meant.
 * The attributes are derivable, of course, but only after all derivations
 * have been read (so that we don't miss one) *and* after all tools are
 * known, because from that we know *what* attributes we have to look for.
 * Fix the possibly incomplete output specification of a tool definition
 * by doing derivations for an anonymous object.
 */
PRIVATE struct slist *AnonList = NULL; /* only contains the object Anonymous */

PRIVATE void
FixOutputClasses(tool, io_attrs)
struct tool *tool;
struct slist *io_attrs;
{
    register arrow_list alist, attrs;
    register struct cons *cons;

    for (cons = Head(tool->tl_scope->sc_scope); cons != NULL;
	 cons = Next(cons)) {
	register struct param *par = ItemP(cons, param);
	register struct type *t = par->par_type;
	
	if ((t->tp_indicator & T_OUT) != 0) {
	    if (par->par_type->tp_attributes != NULL) {
		attrs = par->par_type->tp_attributes->e_list;
	    } else {
		attrs = NULL;
	    }
	    RemoveAttributes(Anonymous);
	    AddAttrList(ObjectExpr(Anonymous), attrs);
	    DetermineRelevantAttrValues(AnonList, io_attrs);

	    DBUG_EXECUTE("class", {
		DBUG_WRITE(("old attrs: ")); DumpAttrValues(attrs); DBUG_NL();
	    });

	    alist = AttrBindings(ObjectExpr(Anonymous), io_attrs);

	    DBUG_EXECUTE("class", {
		DBUG_WRITE(("new attrs:")); DumpAttrValues(alist); DBUG_NL();
	    });

	    /* now replace it */
	    par->par_type->tp_attributes->e_list = alist;
	}
    }
}

PUBLIC void
InitClasses()
{
    /* when everything has been read, we can create the global graph */
    GlobalGraph = new_graph();
    Append(&AnonList, ObjectExpr(Anonymous));

    /* IO_special_attributes also contains a few (not all?) selected special
     * attributes, which are not contained in IO_attributes.
     */
    Append(&IO_special_attributes, Id_persistent);
    Append(&IO_special_attributes, Id_implicit);
    Append(&IO_special_attributes, Id_compare);

    ITERATE(DeclaredTools, tool, t, {
	FixOutputClasses(t, IO_special_attributes);
	PutInvocationInGraph(GlobalGraph, MakeCallExpr(t->tl_idp), 0);
    });
}

PUBLIC void
PutObjectsInGraph(graph, objects)
struct graph *graph;
struct slist *objects;
{
    /* Determine the class for each object by evaluating getting all
     * the relevant attributes. As this may involve calling some tools
     * we'll have to ITERATE over them until all attributes are known.
     * Where do we check that sources and targets don't belong to the 
     * same class; or should we just let the algorithm find out?
     */
    DetermineRelevantAttrValues(objects, IO_attributes);

    DBUG_EXECUTE("objects", {
	DBUG_WRITE(("objects:")); DBUG_NL();
	ITERATE(objects, expr, obj, {
	    PrintObject(obj->e_object);
	});
    });

    ITERATE(objects, expr, obj_expr, {
        register arrow_list alist = AttrBindings(obj_expr, IO_attributes);
	register struct objclass *class = AddClass(&graph->gr_classes, alist);

	DBUG_EXECUTE("objects", {
	    DBUG_WRITE(("`%s' is put in class ",
			SystemName(obj_expr->e_object)));
	    PrintClass(class); DBUG_NL();
	});
	if (IsEmpty(class->cs_attr_values)) {
	    Warning("`%s' is put in unrestricted class",
		    SystemName(obj_expr->e_object));
	}
        Append(&class->cs_objects, obj_expr);
	put_expr_list(alist);
    });
}

PRIVATE class_list
CopyClassStructure(classes)
class_list classes;
{
    /* Each cluster gets its own copy of the generic class structure,
     * in which it fills in the sources and targets.
     * This graph may then be altered acording to the various rules
     * described below
     */
    class_list copy = NULL;

    /* create the classes */
    ITERATE(classes, objclass, c, {
	c->cs_copy = new_objclass();
	c->cs_copy->cs_attr_values = CopyAttrBindings(c->cs_attr_values);
	Append(&copy, c->cs_copy);
    });

    /* add the sub- and superclass structure */
    ITERATE(classes, objclass, c, {
	ITERATE(c->cs_supers, objclass, super, {
	    Append(&c->cs_copy->cs_supers, super->cs_copy);
	});
	ITERATE(c->cs_subs, objclass, sub, {
	    Append(&c->cs_copy->cs_subs, sub->cs_copy);
	});
    });

    return(copy);
}

PUBLIC void
InitGraph(graphp)
struct graph **graphp;
{
    *graphp = new_graph();
    (*graphp)->gr_classes = CopyClassStructure(GlobalGraph->gr_classes);
}

/*
 * Building up the tool/class-hierarchy.
 * For the moment assume the output classes are also known.
 * (Currently this is the case for all the installed .amk files.)
 *
 * Possible approach: each time we read a tooldef, we create
 * in and output classes (if not already present), and make the links
 * necessary. Note that creation of sub/superclasses later on doesn't
 * alter this binding: the cluster algorithm looks for conflicts when
 * all is known.
 * Important decision: do clusters share this hierarchy?
 * As the algorithm is not necessarily ready in one turn (we might need
 * to invoke tools along the way), we would prefer having a means
 * to keep a per-cluster administration.
 * Making a copy of the cluster hierarchy is also possible of course.
 * Let's take this approach for the moment.
 *
 * Given the hierarchy, we have to spread the sources and targets
 * over the classes. After that we have to prune down the graph until
 * only filled classes are available.
 *
 * To reach this, we have a couple of rules available, that change
 * the state of the graph. These rules must be `safe' in that they
 * may not make a feasible task-model impossible.
 * The order of application of the rules shouldn't matter, though
 * efficiency reasons will probably cause us to make a hierarchy
 * between the rules also (sounds more difficult than it is, really).
 *
 * Rules are be of the form:
 *  " RULE i: IF Condition(graph) THEN graph := Operator(graph) "
 */

/*
 * RULE 1 (prune useless tool top-down or bottom up):
 *
 * IF T is a tool with
 *
 * for *each* class C in the output classes of T: EmptyLeafClass(C, T_IN),
 * i.e.,
 *
 * (1) C contains no (target?) objects and
 * (2) C is not the input class of another tool and
 * (3) C is not the subclass of another class
 *
 * OR
 *
 * for *any* class C in the input classes of T: EmptyLeafClass(C, T_OUT), i.e.,
 *
 * (1) C contains no (source or intermediate) objects and
 * (2) C is not the output class of another tool and
 * (3) C is not the superclass of another class
 *
 * THEN
 * remove T from the graph
 * (we might also remove some classes, but it is probably best to do this
 *  with a general rule, that may be of use in other situations).
 */

PRIVATE int
EmptyLeafClass(class, direction)
struct objclass *class;
int direction;
{
    if (IsEmpty(class->cs_objects)) {
	if (direction == T_IN) {
	    return(IsEmpty(class->cs_subs) &&
		   IsEmpty(class->cs_output_tools));
	} else {
	    assert(direction == T_OUT);
	    return(IsEmpty(class->cs_supers) &&
		   IsEmpty(class->cs_input_tools) &&
		   IsEmpty(class->cs_persistent_tools));
	}
    } else {
	return(FALSE);
    }
}

PRIVATE int
ToolCanBeRemoved(inst)
struct toolinstance *inst;
{
    ITERATE(inst->ti_header->e_args, expr, arrow, {
	register struct param *par = arrow->e_left->e_param;
	int inout = par->par_type->tp_indicator & (T_IN | T_OUT);

	if (inout == T_IN) {
	    if (par->par_default == NULL &&
		EmptyLeafClass(par->par_class, T_IN)) {
		Verbose(1, "tool `%s' not used because `%s' can't be filled",
			inst->ti_tool->tl_idp->id_text,
			par->par_idp->id_text);
		return(TRUE);
	    }
	} else if (inout == T_OUT) {
	    if (!EmptyLeafClass(par->par_class, T_OUT)) {
		Verbose(2, "don't remove tool `%s' because `%s' may be used",
			inst->ti_tool->tl_idp->id_text,
			par->par_idp->id_text);
		return(FALSE);
	    }
	} else if (par->par_default == NULL) {
	    Verbose(1, "tool `%s' not used because `%s' has no default",
		    inst->ti_tool->tl_idp->id_text,
		    par->par_idp->id_text);
	    return(TRUE);
	}
    });

    Verbose(1, "tool `%s' not used because none of the outputs is needed",
	    inst->ti_tool->tl_idp->id_text);
    return(TRUE);
}

PRIVATE void
RmToolFromClass(ti, c, inout)
struct toolinstance *ti;
struct objclass *c;
int inout;
{
    if (inout == T_IN) {
	ITERATE(c->cs_input_tools, toolinstance, t, {
	    if (t == ti) {
		Remove(&c->cs_input_tools, cons);
		return;
	    }
	});
	ITERATE(c->cs_persistent_tools, toolinstance, t, {
	    if (t == ti) {
		Remove(&c->cs_persistent_tools, cons);
		return;
	    }
	});
    } else {
	ITERATE(c->cs_output_tools, toolinstance, t, {
	    if (t == ti) {
		Remove(&c->cs_output_tools, cons);
		return;
	    }
	});
    }
    FatalError("tool `%s' couldn't be removed from class",
	       ti->ti_tool->tl_idp->id_text);
}


PRIVATE void
RmToolFromGraph(ti)
struct toolinstance *ti;
{
    ITERATE(ti->ti_header->e_args, expr, arrow, {
	register struct param *par = arrow->e_left->e_param;
	int inout = par->par_type->tp_indicator & (T_IN | T_OUT);

	if (inout != 0) {
	    RmToolFromClass(ti, par->par_class, inout);
	}
    });
}

PRIVATE int
PruneTools(graph)
struct graph *graph;
{
    int removed = FALSE;
    register struct cons *cons, *next;

    for (cons = Head(graph->gr_tools); cons != NULL; ) {
	register struct toolinstance *ti = ItemP(cons, toolinstance);

	next = Next(cons);
	if (ToolCanBeRemoved(ti)) {
	    removed = TRUE;
	    Remove(&graph->gr_tools, cons);
	    RmToolFromGraph(ti);
	}
	cons = next;
    }
    return(removed);
}

/*
 * RULE 2: remove unused class, top down (bottom up)
 *
 * IF C is a class which
 * (1) contains no objects
 * (2) is not the input- or outputclass of any tool
 * (3) has no super (sub) classes
 *
 * THEN
 * remove C from the graph
 */

PRIVATE void
DoRemoveClass(class)
struct objclass *class;
{
    register struct cons *cons, *next;

    assert(IsEmpty(class->cs_input_tools));
    assert(IsEmpty(class->cs_persistent_tools));
    assert(IsEmpty(class->cs_output_tools));

/*
    ITERATE(class->cs_subs, objclass, sub, {
	RemoveLink(sub, class);
    });
    ITERATE(class->cs_supers, objclass, super, {
	RemoveLink(class, super);
    });
 */

    for (cons = Head(class->cs_subs); cons != NULL; ) {
	register struct objclass *sub = ItemP(cons, objclass);

	next = Next(cons);
	RemoveLink(sub, class);
	cons = next;
    }
    for (cons = Head(class->cs_supers); cons != NULL; ) {
	register struct objclass *super = ItemP(cons, objclass);

	next = Next(cons);
	RemoveLink(class, super);
	cons = next;
    }

    put_expr_list(class->cs_attr_values);
    free_objclass(class);
}

PRIVATE int
ClassCanBeRemoved(c)
struct objclass *c;
{
    return(IsEmpty(c->cs_objects) &&
	   IsEmpty(c->cs_input_tools) &&
	   IsEmpty(c->cs_persistent_tools) &&
	   IsEmpty(c->cs_output_tools) &&
	   (IsEmpty(c->cs_subs) || IsEmpty(c->cs_supers)));
}

PRIVATE int
PruneClasses(graph)
struct graph *graph;
{
    int removed = FALSE;
    register struct cons *cons, *next;

    for (cons = Head(graph->gr_classes); cons != NULL; ) {
	register struct objclass *c = ItemP(cons, objclass);

	next = Next(cons);
	if (ClassCanBeRemoved(c)) {
	    removed = TRUE;
	    DBUG_EXECUTE("prune", {
		DBUG_WRITE(("remove class ")); PrintClassInfo(c); DBUG_NL();
	    });
	    DoRemoveClass(c);
	    Remove(&graph->gr_classes, cons);
	}
	cons = next;
    }
    return(removed);
}

/*
 * RULE 3 (move objects to superclass)
 *
 * IF C is a class which
 * (1) contains > 0 objects, all of which are non-targets
 * (2) has exactly one superclass
 * (3) has no tool using it as input
 *
 * THEN
 * move the objects in C to its superset.
 */

/*ARGSUSED*/
PRIVATE int
MoveObjects(graph)
struct graph *graph;
{
    return(FALSE);
}

/*
 * RULE 4 (choice rule, using priorities introduced by %use clause)
 */
enum removable { DID_REMOVE, DIDNT_REMOVE, NO_PROBLEM };


PRIVATE void
PrintAttrSpec(c)
struct objclass *c;
{
    int first = TRUE;
    WriterFunc old_writer = Write;

    Write = (WriterFunc) PrintError;
    PrintError("[");
    ITERATE(c->cs_attr_values, expr, spec, {
	PrintError("%s%s=", first ? "" : ", ", spec->e_left->e_idp->id_text);
	PrExpr(spec->e_right);
	first = FALSE;
    });
    PrintError("]");
    Write = old_writer; 
}

PRIVATE enum removable
RmInconsistency(graph, c, report)
struct graph *graph;
struct objclass *c;
int report;
{
    int num = 0, high = -1, several = FALSE;
    struct toolinstance *high_prio = NULL;

    if (IsEmpty(c->cs_output_tools) && IsEmpty(c->cs_objects)) {
	/* if the class will never contain any objects, it won't
	 * introduce any problems, because apparently all tools using it
	 * as input have a default for it. This is the case for
	 * inputclasses like [type = command]
	 */
	return(NO_PROBLEM);
    }

    ITERATE(c->cs_input_tools, toolinstance, t, {
	num++;
	if (t->ti_index > high) {
	    high_prio = t;
	    high = t->ti_index;
	    several = FALSE;
	} else if (t->ti_index == high) {
	    several = TRUE; /* several conflicting tools with same priority? */
	}
    });

    if (num <= 1) {
	/* one tool or less so no inconsistencies */
	return(NO_PROBLEM);
    } else if (several) {
	/* we can't choose between several tools with same priority */
	if (report) {
	    int first = TRUE;

	    PrintError("conflict: input class ");
	    PrintAttrSpec(c);
	    PrintError(" used by:");
	    ITERATE(c->cs_input_tools, toolinstance, ti, {
		if (ti->ti_index == high) {
		    PrintError("%s %s", first ? "" : "," ,
			       ti->ti_tool->tl_idp->id_text);
		    first = FALSE;
		}
	    });
	    PrintError("\n");
	}
	return(DIDNT_REMOVE);
    } else {
	assert(high_prio != NULL);
	/* remove all the tools except the one with the highest priority */

	ITERATE(c->cs_input_tools, toolinstance, ti, {
	    if (ti != high_prio) {
		Verbose(1, "tool `%s' preferred above `%s'",
			high_prio->ti_tool->tl_idp->id_text,
			ti->ti_tool->tl_idp->id_text);
		RmToolFromGraph(ti);
		ITERATE(graph->gr_tools, toolinstance, t, {
		    if (t == ti) {
			Remove(&graph->gr_tools, cons);
			break;
		    }
		});
	    }
	});
	return(DID_REMOVE);
    }
}

enum consistency { INCONSISTENT, TRY_AGAIN, CONSISTENT };

PRIVATE enum consistency
RemoveInconsistencies(graph, report)
struct graph *graph;
int report;
{
    enum removable rm;
    int removed = FALSE, inconsistent = FALSE;
    
    /*
     * Try to remove remaining inconsistencies in the tool graph.
     * More specifically: when one class is used by more than one
     * tool, we choose the one specified in a use clause.
     * If none of them is specified: give an error diagnostic, so that the
     * user knows which choices are available.
     */
    ITERATE(graph->gr_classes, objclass, c, {
	if ((rm = RmInconsistency(graph, c, report)) == DID_REMOVE) {
	    removed = TRUE;
	} else if (rm == DIDNT_REMOVE) {
	    inconsistent = TRUE;
	}
    });

    if (inconsistent) {
	return(INCONSISTENT);
    } else if (removed) {
	return(TRY_AGAIN);
    } else {
	return(CONSISTENT);
    }
}


/* We already have an object class-hierarchy, but we have yet to check
 * that there are no loops in the tool graph. Algorithm:
 *
 * while (unmarked tools exist) {
 *    if (an unmarked tool whose input classes are either marked only
 *        with C_INPUT, or who are produced by a marked tool) {
 *        mark that tool;
 *    } else {
 *        failure: there is a cycle in the graph;
 *    }
 * }
 */
    
PRIVATE int
UnmarkedLowerTool(c)
struct objclass *c;
{
    ITERATE(c->cs_output_tools, toolinstance, ti, {
	if ((ti->ti_flags & TI_MARKED) == 0) {
	    return(TRUE);
	}
    });
    ITERATE(c->cs_subs, objclass, sub, {
	if (UnmarkedLowerTool(sub)) {
	    return(TRUE);
	}
    });
    return(FALSE);
}

PRIVATE int
BottomTool(ti)
struct toolinstance *ti;
{
    ITERATE(ti->ti_header->e_args, expr, arrow, {
	register struct param *par = arrow->e_left->e_param;
	register int inout = par->par_type->tp_indicator & (T_IN | T_OUT);

	if (inout == T_IN) {
	    if ((par->par_class->cs_flags & (C_INPUT|C_OUTPUT)) != C_INPUT &&
		UnmarkedLowerTool(par->par_class)) {
		/* not a bottom tool */
		return(FALSE);
	    }
	}
    });
    return(TRUE);
}

PRIVATE int
CheckToolHierarchy(graph)
struct graph *graph;
/* sorts tools in graph according to their inputclasses, if possible */
{
    struct slist *ordered = NULL;

    while (!IsEmpty(graph->gr_tools)) {
	int marked = FALSE;

	ITERATE(graph->gr_tools, toolinstance, ti, {
	    if (BottomTool(ti)) {
		ti->ti_flags |= TI_MARKED;
		marked = TRUE;
		Remove(&graph->gr_tools, cons);
		Append(&ordered, ti);
		break;
	    }
	});
	if (!marked) {
	    int first = TRUE;

	    PrintError("cannot put the following tools in an hierarchy:");
	    ITERATE(graph->gr_tools, toolinstance, ti, {
		PrintError("%s %s", first ? "" : "," ,
			   ti->ti_tool->tl_idp->id_text);
		first = FALSE;
	    });
	    PrintError("\n");
	    return(FALSE);
	}
    }

    graph->gr_tools = ordered;
    return(TRUE);
}

/*ARGSUSED*/
PUBLIC int
PruneGraph(graph, clus)
struct graph *graph;
struct cluster *clus;
{
    enum consistency res;

    if (IsEmpty(graph->gr_tools)) {
	SemanticError("No tools provided, so I don't know what to do");
	return(FALSE);
    }

    do {
	int r1, r2, r3;

	do {
	    r1 = PruneTools(graph); 	/* rule 1 */
	    DBUG_PRINT("class", ("graph after tool removal"));
	    PrintGraph(graph);
	    r2 = PruneClasses(graph);	/* rule 2 */
	    DBUG_PRINT("class", ("graph after class removal"));
	    PrintGraph(graph);
	    r3 = MoveObjects(graph);	/* rule 3 */
	} while (r1 || r2 || r3); /* stop when graph didn't change */
	PrintGraph(graph);
    }
    while ((res = RemoveInconsistencies(graph, FALSE)) == TRY_AGAIN);
	    
    if (res == INCONSISTENT) {
	PrintError("not enough information to decide what to do:\n");
	(void) RemoveInconsistencies(graph, TRUE);
    }

    if (IsEmpty(graph->gr_tools)) {
	PrintError("No tools left! I suggest you run Amake verbose.\n");
	return(FALSE);
    }

    /* Check if all sources and targets lie in a input and output class,
     * respectively.
     */
    ITERATE(graph->gr_tools, toolinstance, ti, {
	ITERATE(ti->ti_header->e_args, expr, arrow, {
	    register struct param *par = arrow->e_left->e_param;
	    int inout = par->par_type->tp_indicator & (T_IN | T_OUT);
		
	    if (inout == T_IN) {
		par->par_class->cs_flags |= C_INPUT;
		MarkSub(par->par_class, C_INPUT);
	    } else if (inout == T_OUT) {
		par->par_class->cs_flags |= C_OUTPUT;
		MarkSuper(par->par_class, C_OUTPUT);
	    }
	});
    });
    ITERATE(graph->gr_classes, objclass, c, {
	ITERATE(c->cs_objects, expr, obj, {
	    if (IsTarget(obj)) {
		if ((c->cs_flags & C_OUTPUT) == 0 ||
		    (c->cs_flags & C_INPUT) != 0) {
		 SemanticError("target `%s' doesn't belong to an output class",
			       SystemName(obj->e_object));
		 return(FALSE);
		}
	    } else {
		if ((c->cs_flags & C_INPUT) == 0) {
		 SemanticError("source `%s' doesn't belong to any input class",
			       SystemName(obj->e_object));
		 return(FALSE);
		}
	    }
	});
    });

    if (res == CONSISTENT && !CheckToolHierarchy(graph)) {
	return(FALSE);
    }

    return(res == CONSISTENT);
}

PUBLIC void
ChangeToolOrder(clus)
struct cluster *clus;
/* For the moment just use the OLD cluster algorithm to spawn commands.
 * Note that we have enough information to do it based on the graph
 * datastructure itself, but that requires a quite some rewriting.
 * Before we open that can of worms, lets implement something that will
 * work almost for free.
 */
{
    struct slist *new_tool_list = NULL;

    ITERATE(clus->cl_graph->gr_tools, toolinstance, ti, {
	Append(&new_tool_list, ti->ti_header);
    });
    FreeSlist(&clus->cl_tool_list);
    clus->cl_tool_list = new_tool_list;
}

