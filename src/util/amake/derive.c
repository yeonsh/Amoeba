/*	@(#)derive.c	1.3	96/02/27 13:05:53 */
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
#include "expr.h"
#include "slist.h"
#include "objects.h"
#include "builtin.h"
#include "eval.h"
#include "scope.h"
#include "error.h"
#include "dbug.h"
#include "type.h"
#include "execute.h"
#include "Lpars.h"
#include "main.h"
#include "derive.h"

PRIVATE int ClassDerivation;

/* Keep info about the (object, attribute) pair we are currently deriving.
 * We then can warn the user about (simple) loops in derivations.
 */
PRIVATE struct expr *CurrentObject;
PRIVATE struct idf  *CurrentAttribute;


/* derivation job info */
struct dj {
    struct expr	*dj_object;	/* object for which the derivation is done  */
    struct idf	*dj_attribute;	/* the attribute we want to derive	    */
    struct cons	*dj_derivation;	/* the current derivation rule		    */
    struct expr *dj_copy_spec;	/* a copy of the [attr, val]* specification */
    struct expr	*dj_condition;  /* instantiation of derivation condition    */
    int		 dj_scope_nr;	/* scope in which the derivation started    */
};

/* allocation definitions of struct dj */
#include "structdef.h"
DEF_STRUCT(struct dj, h_dj, cnt_dj)
#define new_dj()	NEW_STRUCT(struct dj, h_dj, cnt_dj, 5)
#define free_dj(p)	FREE_STRUCT(struct dj, h_dj, p)


#ifdef DEBUG
PUBLIC void
DerivationStructReport()
{
    StructReport("dj", sizeof(struct dj), cnt_dj, h_dj);
}
#endif

/*
 *  Call once:
 *	the_object = ObjectExpr(obj);
 *      cond = DeriveInst(deriv, the_object, deriv->dr_condition);
 *  Until completed:
 *	val = TryDerivation(the_object, cond, deriv, attr);
 */

PRIVATE struct expr *
DeriveInst(deriv, obj_expr, expr)
struct derivation *deriv;
struct expr *obj_expr;
struct expr *expr;
{
    struct expr *instance;
    struct param *der_obj;

    AddScope(deriv->dr_var);

    /* check that the scope contains exactly one element */
    assert(!IsEmpty(deriv));

    der_obj = ItemP(HeadOf(deriv->dr_var->sc_scope), param);
    assert(der_obj != NULL);
    assert(Next(HeadOf(deriv->dr_var->sc_scope)) == NULL);

    der_obj->par_value = obj_expr;
    
    instance = CopyExpr(expr);	/* obj_expr filled in */
    RemoveScope();
    return(instance);
}

PRIVATE struct expr *
TryDerivation(dj)
struct dj *dj;
{
    register struct cons *cs;
    register struct expr *val, *spec, *attr_val = NULL;
    struct derivation *deriv = ItemP(dj->dj_derivation, derivation);

    if (IsTrue(dj->dj_condition = Eval(dj->dj_condition))) {
	/* OK. Add attributes specified in this derivation globally.
	 * attr is the one we have to return.
	 */

	/* first make a  private copy of the attribute specifications, if it
	 * is not already done.
	 */
	if (dj->dj_copy_spec == NULL) {
	    dj->dj_copy_spec = DeriveInst(deriv, dj->dj_object,
					  deriv->dr_attr_spec);
	}

	/* Now evaluate the attributes, this may have to be retried */
	for (cs = Head(dj->dj_copy_spec->e_list); cs != NULL; cs = Next(cs)) {
	    spec = ItemP(cs, expr);
	    spec->e_right = Eval(spec->e_right);
	}

	for (cs = Head(dj->dj_copy_spec->e_list); cs != NULL; cs = Next(cs)) {
	    register struct idf *at;

	    spec = ItemP(cs, expr);
	    at = get_id(spec->e_left);
	    val = CopyExpr(spec->e_right);

	    DBUG_EXECUTE("attr", {
		DBUG_WRITE(("attribute %s has value ", at->id_text));
		DBUG_Expr(val); DBUG_NL();
	    });
	    
	    PutAttrValue(dj->dj_object->e_object, at, val, dj->dj_scope_nr);

	    if (at == dj->dj_attribute) {
		attr_val = val;		/* to be returned */
	    }
	}
	/* and clean up private attr_spec copy */
	put_expr(dj->dj_copy_spec);
	dj->dj_copy_spec = NULL;

	assert(attr_val != NULL);	/* must have been assigned to */
    } else {
	DBUG_PRINT("attr", ("derivation failed"));
    }
    return(attr_val);
}

PRIVATE struct expr *
ReallyDoDerive(dj)
struct dj *dj;
/*
 * Try to derive the value of attribute 'attr' for object 'obj'
 * attr->id_attr_descr->ad_derivations is an slist of derivations that may
 * be tried.
 * When the derivation succeeds, DerivationResult is set to the value found.
 * Because it is possible/likely that some object occurs as source in more
 * than one cluster, we must also make sure that the same attribute is not
 * derived twice.
 */
{
    struct expr *val;

    /* check for simple loops when a BUSY node is encountered */
    CurrentObject = dj->dj_object;
    CurrentAttribute = dj->dj_attribute;

    while (dj->dj_derivation != NULL) {
	if ((val = TryDerivation(dj)) != NULL) {
	    break;
	}

	if ((dj->dj_derivation = Next(dj->dj_derivation)) != NULL) {
	    struct derivation *deriv = ItemP(dj->dj_derivation, derivation);

	    dj->dj_condition = DeriveInst(deriv, dj->dj_object,
					  deriv->dr_condition);
	}
    }
    if (dj->dj_derivation == NULL) {
	DBUG_PRINT("attr", ("all derivations failed"));
	val = UnKnown;
	PutAttrValue(dj->dj_object->e_object, dj->dj_attribute,
		     val, dj->dj_scope_nr);
    } /* else the PutAttrValue has already happened */

    return(val);	/* only used when derivation didn't suspend */
}

PRIVATE struct job *
DeriveJob(obj, attr)
struct object *obj;
struct idf *attr;
{
    struct expr *busy;
    struct job *job;
    struct dj *dj;
    struct derivation *deriv;

    assert(attr->id_attr_descr != NULL && !IsEmpty(attr->id_attr_descr->ad_derivations));

    dj = new_dj();
    dj->dj_object = ObjectExpr(obj);
    dj->dj_attribute = attr;
    dj->dj_derivation = Head(attr->id_attr_descr->ad_derivations);
    deriv = ItemP(dj->dj_derivation, derivation);
    dj->dj_condition = DeriveInst(deriv, dj->dj_object, deriv->dr_condition);
    dj->dj_copy_spec = NULL;
    dj->dj_scope_nr = AttributeScopeNr;

    job = CreateJob(ClassDerivation, (generic)dj);

    /* let others wait until it is derived */
    busy = new_expr(BUSY);
    busy->e_job = job;
    PutAttrValue(obj, attr, busy, dj->dj_scope_nr);

    return(job);
}

PUBLIC struct expr *
HandleBusy(busy)
struct expr *busy;
{
    struct dj *dj = (struct dj *)JobInfo(busy->e_job);

    if (dj->dj_attribute == CurrentAttribute &&
	dj->dj_object == CurrentObject) {
	Warning("derivation loop while deriving `%s' for `%s'",
		CurrentAttribute->id_text,
		SystemName(CurrentObject->e_object));
	PutAttrValue(CurrentObject->e_object, CurrentAttribute,
		     UnKnown, dj->dj_scope_nr);
	return(UnKnown);
    } else {
	Verbose(1, "suspended on derivation of attribute `%s' of `%s'",
		dj->dj_attribute->id_text,
		SystemName(dj->dj_object->e_object));
	SuspendCurrent(busy->e_job);
	ReSched();
	/*NOTREACHED*/
    }
}

PUBLIC struct expr *
Derive(obj, attr)
struct object *obj;
struct idf *attr;
{
    struct job *deriv, *save;
    struct expr *val;

    assert(attr->id_attr_descr != NULL);
    
    /* If there are no derivations for this attribute, no need to try it. */
    if (IsEmpty(attr->id_attr_descr->ad_derivations)) {
	/* No need to explicitly set the value to unknown, because that is
	 * the default. It also saves memory.
	 */
	return(UnKnown);
    }

    deriv = DeriveJob(obj, attr);
    save = InsertJob(deriv);

    val = ReallyDoDerive((struct dj *) JobInfo(deriv));

    /* If we come here, the derivation could be completed directly
     * Now undo any changes made.
     */

    /* note that 'save' must NOT be put in ReadyQ, we just return to it */
    RemoveInsertedJob(save);

    return(val);
}

PUBLIC void
EnterDerivation(deriv)
/*
 * Attach this derivation to the attributes for which it derives a value.
 * To Do: Check that all derivations of some attribute derive a value of
 * the same type.
 */
struct derivation *deriv;
{
    struct cons *cons;
    struct idf *idp;

    for (cons = Head(deriv->dr_attr_spec->e_list);
	 cons != NULL; cons = Next(cons)) {
	idp = get_id(ItemP(cons, expr)->e_left);
	DBUG_PRINT("attr", ("linked derivation to %s", idp->id_text));
	assert(idp->id_attr_descr != NULL);
	Append(&idp->id_attr_descr->ad_derivations, deriv);
    }
}

PRIVATE void
RmDerivation(dj)
struct dj *dj;
{
    put_expr(dj->dj_object);
    free_dj(dj);
}

PRIVATE char *
PrDerivation(dj)
struct dj *dj;
{
    (void)sprintf(ContextString, "derivation of `%s' for `%s'",
		 dj->dj_attribute->id_text,
		 SystemName(dj->dj_object->e_object));
    return(ContextString);
}

PRIVATE void
DoDerive(dj)
struct dj **dj;
{
    (void) ReallyDoDerive(*dj);
}

PUBLIC void
InitDerive()
{
    ClassDerivation = NewJobClass("derive", (void_func) DoDerive, 
			 (void_func) RmDerivation, (string_func) PrDerivation);
}

#ifdef DEBUG

PRIVATE void
print_one(deriv)
struct derivation *deriv;
{
    DBUG_EXECUTE("derive", {
	DBUG_WRITE(("# DERIVE $%s[",
	    ItemP(HeadOf(deriv->dr_var->sc_scope),param)->par_idp->id_text));
	DBUG_Expr(deriv->dr_attr_spec);
	DBUG_WRITE(("] WHEN "));
	DBUG_Expr(deriv->dr_condition);
	DBUG_NL();
    });
}

PUBLIC void
PrintDerivations(attr)
/*
 * Print out the derivations delivering a value.
 */
struct idf *attr;
{
    DBUG_EXECUTE("derive", {
	DBUG_NL();
	DBUG_WRITE(("# Derivations for %s #", attr->id_text));
	DBUG_NL();
	assert(attr->id_attr_descr != NULL);
	Iterate(attr->id_attr_descr->ad_derivations, (Iterator) print_one);
	DBUG_NL();
    });
}

#endif /* DEBUG */
