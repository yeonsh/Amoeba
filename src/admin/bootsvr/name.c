/*	@(#)name.c	1.5	94/04/06 11:39:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "svr.h"

/*
 *	This module maps names to indices in objs[] and objnums
 *	- MkMap creates the map
 *	- KillMap invalidates the map until MkMap is called
 *	- UniqNum finds an unused objnum
 *	- FindName finds the name and returns the index in objs[]
 *	- FindNum finds the object number and returns the index.
 */

static mutex name_lock;
static int n_names;	/* # entries in name_map */
static struct {
    ident name;
    objnum num;
    int ind;
} name_map[MAX_OBJ];

int
KillMap()
{
    if (debugging) prf("%nKillMap()\n");
    /* Be interruptable: */
    if (mu_trylock(&name_lock, (interval) -1) != 0) return -1;
    return 0;
} /* KillMap */

objnum
UniqNum()
{
    static objnum num = 0;
    int i;
    do {
	++num;
	if ((long) num < 0) num = 0;
	/* See if it's in use */
	for (i = 0; i < n_names && name_map[i].num != num; ++i)
	    ;
    } while (i < n_names);
    return num;
} /* UniqNum */

void
MkMap(objs, n)
    obj_rep objs[];
    int n;
{
    int j;

    /* name_lock should be locked here */
    if (n > MAX_OBJ || mu_trylock(&name_lock, (interval) 0) == 0)
	abort();

    n_names = 0;
    while (--n >= 0) {
	if (obj_in_use(&objs[n])) {
	    (void) strcpy(name_map[n_names].name, objs[n].or.or_data.name);
	    name_map[n_names].num = prv_number(&objs[n].or.or_objcap.cap_priv);
	    name_map[n_names].ind = n;
	    for (j = 0; j < n_names; j++) {
		if (name_map[j].num == name_map[n_names].num) {
		    prf("%nWARNING INCONSISTENCY: name_map[%d].num == name_map[%d].num = %d\n",
				j, n_names, name_map[j].num);
		    prf("%nname_map[%d].ind = %d, name_map[%d].ind = %d\n",
			    j, name_map[j].ind, n_names, name_map[n_names].ind);
		}
	    }
	    ++n_names;
	}
    }
    MU_CHECK(name_lock);
    mu_unlock(&name_lock);
} /* MkMap */

int
FindName(name)
    ident name;
{
    int ind;
    if (name[0] == '\0') {
	if (debugging) prf("%nFindName: looking for null name\n");
	return -1;
    }
    /* Be interruptable: */
    if (mu_trylock(&name_lock, (interval) -1) != 0) return -1;
    for (ind = n_names; --ind >= 0 && strcmp(name, name_map[ind].name) != 0; )
	;
    MU_CHECK(name_lock);
    mu_unlock(&name_lock);
    return (ind < 0) ? -1 : name_map[ind].ind;
} /* FindName */

int
FindNum(num)
    objnum num;
{
    int ind;

    if (mu_trylock(&name_lock, (interval) -1) != 0) return -1;
    for (ind = n_names; --ind >= 0 && num != name_map[ind].num; )
	;
    MU_CHECK(name_lock);
    mu_unlock(&name_lock);
    if (ind < 0)
	return -1;
    else
	return name_map[ind].ind;
} /* FindNum */
