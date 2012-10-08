/*	@(#)derive.h	1.2	94/04/07 14:48:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Declarations are directly attached to the objects, although evaluation
 * is possibly suspended until really asked for.
 * Derivations apply to all objects, also the ones not yet known at the time
 * the derivation is defined. A derivation can be seen as an unnamed boolean
 * function having one ('VAR') parameter of type 'object'. When it returns
 * TRUE, the condition was true, and the (possibly valued) attributes are
 * attached to the object.
 * Invokation is demand-driven: when Amake wants to know the value/existence
 * of some attribute, it has to invoke 1 or more derivation rules that
 * apply. This is implemented by maintaining for each attribute a list of
 * derivations, to be applied according to the ordering of the list.
 */

struct derivation {
    /* Note: derivations don't have a name. */
    struct scope  *dr_var;		/* containing the only parameter */
    struct expr   *dr_attr_spec;	/* a list of attr_spec records */
    struct expr	  *dr_condition;	/* condition to be evaluated */
};

/* allocation definitions of struct derivation */
#include "structdef.h"
DEF_STRUCT(struct derivation, h_derivation, cnt_derivation)
#define new_derivation()   NEW_STRUCT(struct derivation, h_derivation, cnt_derivation, 10)
#define free_derivation(p) FREE_STRUCT(struct derivation, h_derivation, p)

void DerivationStructReport	P((void));
struct expr *HandleBusy		P((struct expr *busy ));
struct expr *Derive		P((struct object *obj , struct idf *attr ));
void EnterDerivation		P((struct derivation *deriv ));
void InitDerive			P((void));
void PrintDerivations		P((struct idf *attr ));

