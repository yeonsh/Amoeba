/*	@(#)name.c	1.3	96/02/27 12:43:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ail.h"

/*
 *	Find a name in the namelist
 */
struct name *
FindName(id, list)
    char *id;
    struct name *list;
{
    for (;list != NULL; list = list->nm_next)
	if (list->nm_name == id) break;
    return list;
} /* FindName */

/*
 *	Find a name in this class or in one of the inherited classes.
 *	Report success/failure by return.
 *	Note that there is a very good reason for refusing to enter a
 *	name in a class, if an inherited class contains the name yet:
 *	This wouldn't map on C.
 */
Bool
WhereIsName(id, clpp, namep)
    char *id;
    struct class **clpp;
    struct name **namep;
{
    struct clist *cl;
    assert(namep != NULL);
    assert(clpp != NULL);
    *namep = FindName(id, ThisClass->cl_scope);
    if (*namep != NULL) {
	*clpp = ThisClass;
	return Yes;
    }
    /* Try the inherited classes	*/
    for (cl = ThisClass->cl_effinh; cl != NULL; cl = cl->cl_next) {
	*namep = FindName(id, cl->cl_this->cl_scope);
	if (*namep != NULL) {
	    *clpp = cl->cl_this;
	    return Yes;
	}
    }
    /* Not found	*/
    *clpp = NULL;
    return No;
} /* WhereIsName */

/*
 *	Find a typedef somewhere. Needed since typedefs and
 *	SUES might have the same name.
 */
static struct name *
WhereIsID(id, kind)
    char *id;
    int kind;
{
    struct name *nm;
    struct clist *cl;
    nm = FindName(id, ThisClass->cl_scope);
    if (nm != NULL) {
	if (nm->nm_kind != kind)	/* Search further:	*/
	    nm = FindName(id, nm->nm_next);
	if (nm != NULL && nm->nm_kind == kind)
	    return nm;	/* Found	*/
    }
    /* Try the inherited classes	*/
    for (cl = ThisClass->cl_effinh; cl != NULL; cl = cl->cl_next) {
	nm = FindName(id, cl->cl_this->cl_scope);
	if (nm != NULL) {
	    if (nm->nm_kind != kind)
		nm = FindName(id, nm->nm_next);
	    if (nm != NULL && nm->nm_kind == kind)
		return nm;
	}
    }
    /* And finally The Universe	*/
    nm = FindName(id, GlobClass.cl_scope);
    if (nm != NULL) {
	if (nm->nm_kind != kind)	/* Search further:	*/
	    nm = FindName(id, nm->nm_next);
	if (nm != NULL && nm->nm_kind == kind)
	    return nm;	/* Found	*/
    }
    /* Not found	*/
    return NULL;
} /* WhereIsTD */

struct name *
WhereIsTD(id)
    char *id;
{
    return WhereIsID(id, TYPEDEF);
}

struct name *
WhereIsIConst(id)
    char *id;
{
    return WhereIsID(id, INTCONST);
}

struct name *
NewName(id, kind)
    char *id;
    int kind;
{
    struct name *list, **last;
    struct class *clp;
    /* See if it is there yet:	*/
    if (WhereIsName(id, &clp, &list) && kind != TYPEDEF) {
	/* Illegal: defined somewhere	*/
	if (ThisClass == clp)
	    mypf(ERR_HANDLE, "%r attempt to redefine %s\n", id);
	else
	    mypf(ERR_HANDLE, "%r %s yet defined in class %s\n",id,clp->cl_name);
	return NULL;
    }
    /* Create & append	*/
    for (last = &ThisClass->cl_scope; *last != NULL; last = &(*last)->nm_next)
	;
    list = new(struct name);
    list->nm_name = id;
    list->nm_next = NULL;
    list->nm_kind = kind;
    *last = list;
    dbg3("NewName(kind = %d: %p\n", kind, list);
    return list;
} /* NewName */

/*
 *	Try putting an already initialized name in the symbol table
 */
Bool
InsName(np)
    struct name *np;
{
    struct class *clp;
    struct name *list;
    assert(np->nm_kind != TYPEDEF);
    if (WhereIsName(np->nm_name, &clp, &list)) {
	/* Illegal: defined somewhere	*/
	if (ThisClass == clp)
	    mypf(ERR_HANDLE, "%r attempt to redefine %s\n", np->nm_name);
	else
	    mypf(ERR_HANDLE, "%r %s yet defined in class %s\n",
		np->nm_name, clp->cl_name);
	return No;
    }
    np->nm_next = NULL;
    /* Append to symbol table	*/
    if (ThisClass->cl_scope == NULL) {
	ThisClass->cl_scope = np;
    } else {
	for (list = ThisClass->cl_scope;
	    list->nm_next != NULL; list = list->nm_next)
		;
	list->nm_next = np;
    }
    return Yes;
} /* InsName */

/*
 *	Set the value of an integer constant.
 *	I think I'd better make a generic CONST
 *	instead of INTCONST/STRCONST.
 *	Constants that of some enumerated type
 *	need a pointer to the typedescriptor.
 */
void
SetIConst(id, val, type)
    char *id;
    long val;
    struct typdesc *type;
{
    struct name *p;
    assert(ThisClass != NULL);
    p = NewName(id, INTCONST);
    if (p != NULL) {
	p->nm_type = type;
	p->nm_ival = val;
    }
} /* SetIConst */

/*
 *	Set the value of a string constant
 */
void
SetSConst(id, val)
    char *id;
    char *val;
{
    struct name *p;
    assert(ThisClass != NULL);
    p = NewName(id, STRCONST);
    if (p != NULL) {
	p->nm_type = NULL;	/* Not really needed	*/
	p->nm_sval = val;
    }
} /* SetSConst */

void
SetRight(name, value)
    char *name;
    unsigned value;
{
    struct name *p;
    unsigned i;

    assert(ThisClass != NULL);
    /*
     *	Check that the value is a power of two: rights are binary values.
     *	Also, the right must not already be claimed in this class.
     */
    for (i = value; !(i & 1); i = i >> 1)
	;
    if (i != 1)
	mypf(ERR_HANDLE, "%r %s (0x%x) is not a power of two\n", name, value);
    else if (ThisClass->cl_rights & value)
	mypf(ERR_HANDLE, "%r right %s (0x%x) defined again\n", name, value);
    else if (ThisClass->cl_inhrights & value)
	mypf(ERR_HANDLE, "%r right %s (0x%x) already inherited\n", name, value);
    else
	ThisClass->cl_rights |= value;	/* Remember we defined this right */

    p = NewName(name, RIGHTS);
    if (p != NULL) {
	p->nm_type = ModifTyp(tk_int, 0, 0, 1);
	p->nm_ival = value;
    }
} /* SetRight */

int
GetRight(name)
    char *name;
{
    struct class *cl;
    struct name *nm;
    int ret;

    ret = 0;	/* Savest right to return	*/
    if (WhereIsName(name, &cl, &nm))
	if (nm->nm_kind == RIGHTS)
	    ret = nm->nm_ival;
	else mypf(ERR_HANDLE, "%r %s is not a right but a %d\n",
				name, nm->nm_kind);
    else mypf(ERR_HANDLE, "%r right %s unknown\n", name);
    return ret;
} /* GetRight */
