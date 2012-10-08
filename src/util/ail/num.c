/*	@(#)num.c	1.2	94/04/07 14:34:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
/*
 *	This file contains functions that take care
 *	that all operator.h_commands are unique
 *	within a class, and that inherit statements
 *	are correct.
 */

/*
 *	Which h_command-values are defined in the current class.
 *	The inherited classes don't count here.
 */
static int com_val_used[1000];

/*
 *	Invent a unique number for an operator.
 */
int
GetOptrVal()
{
    if (ThisClass->cl_last >= ThisClass->cl_hi) {
	mypf(ERR_HANDLE, "%r out of operator-values for class %s\n",
	    ThisClass->cl_name);
    }
    return ThisClass->cl_last + 1;
} /* GetOptrVal */

/*
 *	Check whether a value for h_command is unique.
 *	Store the checked value in com_val_used for the next checks.
 */
void
CheckOptrVal(val)
    int val;
{
    int i;
    if (val < ThisClass->cl_lo || val > ThisClass->cl_hi) {
	mypf(ERR_HANDLE, "%r Operator value %d outside range %d..%d\n",
	    val, ThisClass->cl_lo, ThisClass->cl_hi);
    } else for (i = 0; i < n_optrs; ++i) {
	if (val == com_val_used[i]) {
mypf(ERR_HANDLE, "%r Operator value %d used before within same class\n", val);
	    break;
	}
    } 
    com_val_used[n_optrs++] = ThisClass->cl_last = val;
} /* CheckOptrVal */

/*
 *	Copy a list of classes
 */
/* static */ struct clist *
CopyClist(l)
    struct clist *l;
{
    struct clist *p;
    p = (struct clist *) NULL;
    for (;l != NULL; l = l->cl_next) {
	struct clist *nw;
	nw = new(struct clist);
	nw->cl_this = l->cl_this;
	nw->cl_next = p;
	p = nw;
    }
    return p;
} /* CopyClist */

/*
 *	Merge class lists l2 in l1.
 *	We keep the classes in each list unique.
 */
/* static */ struct clist *
MergeClist(l1, l2)
    struct clist *l1, *l2;
{
    struct clist *walk1, *walk2, *pw2;
    if (l1 == NULL) return l2;
    if (l2 == NULL) return l1;
    assert(l1 != l2);
    for (walk1 = l1; walk1 != NULL; walk1 = walk1->cl_next) {
	/* See if *walk1 is in l2	*/
	pw2 = (struct clist *) NULL;
	for (walk2 = l2; walk2 != NULL; walk2 = walk2->cl_next) {
	    if (walk1->cl_this->cl_name == walk2->cl_this->cl_name) {
		/* Unlink from l2	*/
		register struct clist *next;
		next = walk2->cl_next;
		if (l2 == walk2) {
		    assert(pw2 == NULL);
		    l2 = next;
		} else {
		    assert(pw2 != NULL);
		    pw2->cl_next = next;
		}
		FREE(walk2);
		break;			/* Can't be there twice		*/
	    }
	    pw2 = walk2;		/* Remember where we were	*/
	}
    }
    /* Attach l2 to the end of l1	*/
    { register struct clist *tail;
    assert(l1 != NULL);	/* We didn't mess there	*/
    for (tail = l1; tail->cl_next != NULL; tail = tail->cl_next) ;
    tail->cl_next = l2; }
    return l1;	/* And return the patched list	*/
} /* MergeClist */

/*
 *	Check that lo1..hi1 doesn't overlap lo2..hi2
 */
static Bool
OverLapping(lo1, hi1, lo2, hi2)
    int lo1, hi1, lo2, hi2;
{
    return
	(lo2 <= lo1 && lo1 <= hi2) ||	/* lo1 is in lo2..hi2	*/
	(lo2 <= hi1 && hi1 <= hi2) ||	/* hi1 is in lo2..hi2	*/
	(lo1 < lo2 && hi1 > hi2);	/* range2 is in range1	*/
} /* OverLapping */

/*
 *	This comment is not just out of date;
 *	it is TERRIBLY out of date.
 *
 *	Scan an inheritance list, translate it
 *	to an effective inheritance list, check
 *	consistency of the range-numbers, return
 *	the list of effectively inherited classes.
 *
 *	The names of constants, types and operators
 *	must be unique, because C is our main target
 *	language. For more modular languages this
 *	looks silly, since names can be qualified:
 *	like in someclass.opaque_type.
 *	On the other hand, C has seperate namespaces
 *	for struct/enum/union tags, typedefs and
 *	constants (well...) which are absent in
 *	PASCAL derivatives.
 *
 *	Solution: everybody looses.
 */
struct clist *
InhStuff(inh)
    struct clist *inh;			/* Inherited classes		*/
{
    struct { int lo, hi; char *name; } values[200];	/* Used values	*/
    int valind;				/* Value index			*/
    struct clist *eff;			/* Effective inheritance	*/
    unsigned inhrights = 0;		/* Inherited rights		*/

    /*
     *	Compute effective inheritance list by merging "inh" and the effective
     *	lists of all the classes in "inh".
     *	MergeClist takes care that the classnames in eff will be unique.
     */
    eff = CopyClist(inh);		/* Inherited by definition	*/
    /* Add the indirectly inherited classes:				*/
    for (; inh != NULL; inh = inh->cl_next) {
	eff = MergeClist(eff, CopyClist(inh->cl_this->cl_effinh));
    }

    /*
     *	Put the information of this class in the tables.
     *	A class doesn't claim any operator numbers if
     *	the CL_NORANGE bit is set in cl_flags.
     *	Valind index in the values array
     */
    if (!(ThisClass->cl_flags & CL_NORANGE)) {
	values[0].lo = ThisClass->cl_lo;
	values[0].hi = ThisClass->cl_hi;
	values[0].name = ThisClass->cl_name;
	valind = 1;
    } else valind = 0;

    /*
     *	Traverse all effective classes, checking the class ranges
     *	and rights. Classes that don't claim operatornumbers don't
     *	count for the first test.
     */
    for (inh = eff; inh != NULL; inh = inh->cl_next) {
	if (!(inh->cl_this->cl_flags & CL_NORANGE)) {
	    /* Only if h_commandvalues are allowed	*/
	    register int tmp;
	    /* Check h_command ranges.	*/
	    for (tmp = 0; tmp < valind; ++tmp) {
		if (OverLapping(values[tmp].lo, values[tmp].hi,
		  inh->cl_this->cl_lo, inh->cl_this->cl_hi)) {
		    mypf(ERR_HANDLE,
			"%r Command range overlap in classes %s and %s\n",
			values[tmp].name, inh->cl_this->cl_name);
		}
	    }
	    /* Add it */
	    values[valind].lo = inh->cl_this->cl_lo;
	    values[valind].hi = inh->cl_this->cl_hi;
	    values[valind].name = inh->cl_this->cl_name;
	    ++valind;
	}
	/* Check that the rights defined here are not already inherited */
	if (inh->cl_this->cl_rights & inhrights) {
	    mypf(ERR_HANDLE, "%r rights bits 0x%x inherited again from %s\n",
		inh->cl_this->cl_rights & inhrights, inh->cl_this->cl_name);
	}
	/* Inherit the rights */
	inhrights |= inh->cl_this->cl_rights;
    }

    /* Remember which rights you inherited */
    ThisClass->cl_inhrights = inhrights;
    return eff;
} /* InhStuff */
