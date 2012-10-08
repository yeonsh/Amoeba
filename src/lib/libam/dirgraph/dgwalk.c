/*	@(#)dgwalk.c	1.7	96/02/27 11:00:57 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* The dgwalk routines create a set of directory graphs that enables
   an amoeba program(mer) to walk through a set of directories.

   Author: E.G. Keizer
	   Faculteit Wiskunde & Informatika
	   Vrije Universiteit
	   Amsterdam


*/

#include <stdlib.h>
#include <amtools.h>
#include <capset.h>
#include <server/soap/soap.h>
#include <sp_dir.h>
#include <server/bullet/bullet.h>
#include <module/dgwalk.h>
#include "dgwdef.h"


/* forward declarations */
static char	*getmem();
static void	dgw_free();
static int	insert();
static int	visit_objects();
static errstat	eatlist();
static int	addpath();
static int	ins_obj();
static errstat	examine();
static int	do_delve();
static int	one_of();
static int	cap_eq();

/* Hash stuff */
static dgw_object	*findobj();
static struct obj_table *obj_table_alloc();

#define MAX_TRIES		20
#define OBJ_EQ(tbl,num,cap2)	(cap_eq(((tbl)->objs[num])->cap,cap2))
#define UNUSED(tbl,num)		(!(tbl)->objs[num])

struct obj_table {
    int		size;		/* Number of entries malloc'd in objs table*/
    int		num_entries;	/* Number of entries in use in objs table */
    dgw_object	**objs;
};


/* The meat of the package
	dgwalk(params)
		mode	ADHOC
			call dodir whenever you find a dir that is
				new
				has better cap's than before and
					is not done yet.
			ALL
			first gather all info on dirs and then dodir().
			testdir is called to see whether a dir should
			be skipped.
		fcap	the starting capability set
		dodir	See mode
		testdir See mode
		walkit	Only usefull in ALL mode. Called instead of own
			graph walking routine.

	RETURNS
		STD_OK	On success
		error	On failure (out of mem)
*/

errstat
dgwalk(params)
	dgw_params *params;
{
	struct dgw_info	my_info;
	errstat		err;
	dgw_paths	*startp;

	my_info.params = *params;
	/* Sanity check */
	if ( !params->dodir ) return STD_ARGBAD ;
	my_info.hasharray = obj_table_alloc(31) ;
	my_info.head = DGW_LNULL ;
	my_info.dofirst.first = (struct memlist *)NULL ;
	my_info.dofirst.last = (struct memlist *)NULL ;
	my_info.doafter.first = (struct memlist *)NULL ;
	my_info.doafter.last = (struct memlist *)NULL ;
	my_info.statmem.first = (struct memlist *)NULL ;
	my_info.statmem.last = (struct memlist *)NULL ;

	for ( startp = params->entry; startp ; startp = startp->next ) {	
		if (!insert(&my_info,&startp->cap,startp->entry,
			       (dgw_paths *)NULL,(dgw_object *)NULL) ) {
			dgw_free(&my_info) ; return STD_NOMEM ;
		}
	}
	/* Now get entry into work list */
	/* Now eat up work lists */
	while ( my_info.dofirst.first || my_info.doafter.first ) {
		err = eatlist(&my_info,&my_info.dofirst) ;
		if ( err != STD_OK ) { dgw_free(&my_info) ; return err ; }
		err = eatlist(&my_info,&my_info.doafter) ;
		if ( err != STD_OK ) { dgw_free(&my_info) ; return err ; } ;
	}

	/* Now visit them all */
	if ( my_info.params.mode&DGW_ALL ) {
		if ( visit_objects(&my_info,my_info.params.dodir) == DGW_QUIT ) {
			dgw_free(&my_info);
			return STD_INTR;
		}
	}
	/* Free all core */
	dgw_free(&my_info);
	return STD_OK ;
}

/* Insert object in graph */
/* I've used a few tricks here.
   All information reachable through an object handle should be stored
   in the static allocation area.
   This means that if we want to replace a path with a better path
   that we have to overwrite the info. The cap pointer in the struct
   points to a cap in one of the paths lists. Thus a capability should
   always be accessible through that pointer.
   There is the odd change that a path coming in might be better than
   two incomparable paths. In that case we scrap the memory used by
   one of the paths, do not reuse it, and free it only at the
   exit of dgwalk.
*/
/* The insertion algorithm is:
   - is it a new object => add object to work list
   - has the path worse rights then any path known => forget it.
   - has the path better or incompatible rights =>
	add object to work list
*/

static int
insert(myp,cap,name,path,parent)
	struct dgw_info		*myp;
	capability		*cap;
	char			*name;
	dgw_paths		*path;
	dgw_object		*parent;
{
	register dgw_object	*obj;
	int			new = 0;
	register dgw_paths	*thispath;
	register long		rights;
	register long		arights;
	int			done;
	register dgw_dirlist	*subdir;

	obj = findobj(myp,cap,&new);
	if ( !obj ) {
		return 0 ;
	}
	if ( new ) {
		obj->paths = (dgw_paths *)0;
		if ( !addpath(myp,obj,cap,name,path) ) return 0 ;
		/* Fill in object */
		obj->subdirs = (dgw_dirlist *)0;
		obj->cap     = &obj->paths->cap;
		/* Add dir to subdirs of parent */
		subdir = getsmem(dgw_dirlist) ;
		if ( !subdir ) return 0 ;
		subdir->dir = obj;
		if ( parent ) {
			subdir->next = parent->subdirs;
			parent->subdirs = subdir;
		} else {
			subdir->next = myp->head;
			myp->head = subdir;
		}
		/* get object on work list */
		return ins_obj(&myp->doafter,obj); /* Poss. out of mem */
	} else {
		if ( parent ) {
			/* Add current to subdirs of parent */
			for ( subdir = parent->subdirs ; subdir ;
						subdir = subdir->next ) {
				if ( cap_eq(cap,subdir->dir->cap) ) break ;
			}
			if ( !subdir ) { /* Not found, insert */
				subdir = getsmem(dgw_dirlist) ;
				if ( !subdir ) return 0 ;
				subdir->next = parent->subdirs;
				subdir->dir = obj;
				parent->subdirs = subdir;
			}
		} /* If parent-less => top element twice in list,
		     thus no further action
		  */	
		if ( obj->flag == DGW__IGNORE ) return 1 ;
	}

	/* Now the hard part. The first check is whether these rights
           are worse or equal to those in any other cap.
	   If so, that other cap has been done or is on the work list,
	   in either case we can forget about this entry.
	   While we are at it, check whether the obj is still on the
	   work list.
	*/
	rights = cap->cap_priv.prv_rights;
	done = 1;
	for( thispath = obj->paths ; thispath ; thispath = thispath->next){
		arights = thispath->cap.cap_priv.prv_rights;
		if ( (arights | rights) == arights ) return 1 ;
		if ( !thispath->done ) done = 0 ;
	}
	
	/* Ok, now we know that this adds rights.
	   Add the new cap to the list and set the flag to not done
	*/
	 /* Poss. out of mem */
	if ( !addpath(myp,obj,cap,name,path) ) return 0 ;
	if ( !done ) return 1 ; /* It's in the work list already */
	return ins_obj(&myp->dofirst,obj);  /* Poss. out of mem */
}

/* Add a new path to the path list.
   Do not delete any path's now. Normalization is
   left to the very end.
   Do replace current entries with better cap's whenever possible.
*/
static int
addpath(myp,obj,cap,name,path)
	struct dgw_info		*myp;
	dgw_object		*obj;
	capability		*cap;
	char			*name;
	dgw_paths		*path;
{
	register char		*name_storage;
	register dgw_paths	*thispath;
	register dgw_paths	*bestpath;
	register dgw_paths	**inpath;
	long			arights,rights;

	/* First check whether name string is already there */
	name_storage = (char *)0;
	for(thispath = obj->paths ; thispath ; thispath = thispath->next){
		if ( strcmp(name,thispath->entry) == 0 ) {
			name_storage = thispath->entry ;
			break ;
		}
	}
	if ( !name_storage ) {
		/* Allocate mem */
		name_storage = getmem(&myp->statmem,strlen(name)+1) ;
		if ( !name_storage ) return 0;
		strcpy(name_storage,name);
	}
	bestpath = (dgw_paths *)NULL ;
	rights = cap->cap_priv.prv_rights;
	thispath = obj->paths;
	inpath = &obj->paths ;
	while ( thispath ) {
		arights = thispath->cap.cap_priv.prv_rights;
		/* We do not have to check for equality.
		   insert does not call us then.
		 */
		if ( (arights | rights) == rights ) {
			/* We avoid replacement the dotdot field of an
			   existing path
			   1- we might create a recursive path
			   2- the path might be part of a short path
			      leading to a capability with max rights.
			*/
			if ( !bestpath && thispath->dotdot == path ) {
				/* replace it */
				bestpath = thispath ;
			} else {
				/* Already found replacement
				   A reference might lead through
				   this path. So we can't throw the
				   storage away. Just remove from list.
				 */
				*inpath = thispath->next;
				thispath = thispath->next;
				continue;
			}
		}
		inpath = &thispath->next;
		thispath = thispath->next;
	}
	if ( !bestpath ) { /* None found, create a new one */
		thispath = getsmem(dgw_paths);
		if ( !thispath ) return 0;
		thispath->next = obj->paths;
		obj->paths = thispath ;
	} else {
		thispath = bestpath;
	}
	thispath->entry = name_storage;
	thispath->dotdot = path;
	thispath->cap = *cap; /* A copy, not a pointer */
	thispath->done = 0;
	return 1 ;
}

/* Add an object to the work list.
   The search algorithm is breadth first. Thus a shortest path
   for a particular capability for each object will be found.
*/
static int
ins_obj(list,obj)
	struct memhead		*list;
	dgw_object		*obj;
{
	register dgw_object	**next;

	next = (dgw_object **)getmem(list,sizeof (dgw_object *));
	if ( !next ) return 0 ; /* Out of mem */
	*next = obj;
	return 1 ;
}

static errstat
eatlist(myp,list)
	struct dgw_info		*myp;
	struct memhead		*list;
{
	register struct memlist	*curlist;
	register char		*pbuf;
	dgw_object		*next;
	int			n_seen;
	int			pobj_size;
	errstat			err;
	
	pobj_size = MEMRND(sizeof (dgw_object *)) ;
	while ( curlist = list->first ) {
		n_seen = 0 ;
		pbuf = (char *)curlist+sizeof(*curlist);
		while ( n_seen<curlist->used ) {
			next = *(dgw_object **) (pbuf+n_seen) ;
			n_seen += pobj_size ;
			err = examine(myp,next) ;
			switch(err) {
			case STD_OK:	break;
			default:	return err;
			}
		}
		/* Ok, we have had a whole buffer now, scratch it */
		list->first = curlist->next ;
		if ( curlist == list->last ) {
			list->last = list->first ;
		}
		free((char *)curlist) ;
	}
	/* We are through, no more work to be done */
	return STD_OK ;
}

/* Examine the contents of a directory */
/* In ALL mode, testdir will be called first */
/* In ADHOC mode, dodir will be called */
static errstat
examine(myp,obj)
	struct dgw_info		*myp;
	dgw_object		*obj;
{
	register dgw_paths	*thispath;
	int			capsdone = 1;
	interval		savetout;
	int			result;
	int			(*docall)();

	if ( obj->flag&(DGW__DONE|DGW__IGNORE) ) return STD_OK;
	/* See whether all cap's have been done and set the cap pointer
	   to the best you can find
	 */
	for(thispath = obj->paths ; thispath ; thispath = thispath->next){
		if ( !thispath->done ) {
			capsdone = 0 ;
		}
	}
	if ( myp->params.mode&DGW_ALL ) {
		if ( myp->params.testdir && !(obj->flag&DGW__TESTED)) {
			if (capsdone&&!(obj->flag&DGW__AGAIN) ) {
				return STD_OK ; /* No new facts to report */
			}
			obj->flag &= ~DGW__AGAIN;
			switch((*myp->params.testdir)(obj->paths)) {
			case DGW_STOP:
				obj->flag |= DGW__IGNORE|DGW__TESTED;
				return STD_OK ;
			case DGW_AGAIN:
				obj->flag |= DGW__AGAIN;
				return ins_obj(&myp->doafter,obj)?STD_OK:STD_NOMEM;
			case DGW_OK:
				obj->flag |= DGW__TESTED;
				break;
			case DGW_QUIT:
				return STD_INTR;
			}
		}
		if ( capsdone ) return STD_OK;
	} else {
		if ( capsdone ) {
			if ( !(obj->flag&DGW__AGAIN) ) return STD_OK ;
		/* If all caps have been done, this is a last chance
		   a better chance might be along, but that is unknown
		   at this point.
		*/
			docall = myp->params.doagain;
		} else {
			docall = myp->params.dodir;
		}
		/* Ad-hoc mode, try it now */
		switch((*docall)(obj->paths)) {
		case DGW_STOP:
			obj->flag |= DGW__IGNORE;
			return STD_OK;
		case DGW_AGAIN:
			obj->flag |= DGW__AGAIN;
			return ins_obj(&myp->doafter,obj)?STD_OK:STD_NOMEM;
		case DGW_OK:
			obj->flag |= DGW__DONE;
			break;
		case DGW_QUIT:
			return STD_INTR;
		}
	}
	/* We let the caller have it's way, now we delve into the dir
	   ourselves.
	*/
	savetout = timeout(PROBE_TIMEOUT);
	result = do_delve(myp,obj);
	(void) timeout(savetout);
	
	return result?STD_OK:STD_NOMEM ;
}

#ifdef DEBUG
#define dbprint(list)   printf list
#else
#define dbprint(list)   /**/
#endif

static errstat
lookup_entries(dd, result)
SP_DIR	   *dd;
sp_result  **result;
{
	sp_entry  *in_list;
	sp_result *out_list;
	int	   row, nrows;
	errstat	   err;

	*result = NULL;

	/* Look up all the entries in a single sp_setlookup(), and add
	 * the ones entries that are directories as well.
	 */
	nrows = dd->dd_nrows;
	if (nrows == 0) {
		/* that's an easy one */
		return STD_OK;
	}

	in_list = (sp_entry *) calloc(sizeof(sp_entry), (size_t) nrows);
	if (in_list == NULL) return STD_NOMEM;

	out_list = (sp_result *) calloc(sizeof(sp_result), (size_t) nrows);
	if (out_list == NULL) {
		free(in_list);
		return STD_NOMEM;
	}

	/* ask for all rows of directory dd */
	for (row = 0; row < nrows; row++) {
		in_list[row].se_capset = dd->dd_capset;
		in_list[row].se_name = dd->dd_rows[row].d_name;
	}
	if ((err = sp_setlookup(nrows, in_list, out_list)) != STD_OK) {
		dbprint(("dg_walk: sp_setlookup failed (%s)\n", err_why(err)));
		free(in_list);
		free(out_list);
		return err;
	}

	/* succeeded */
	*result = out_list;
	free(in_list);
	return STD_OK;
}

static errstat
add_dir_entries(myp, obj, thispath, dd)
struct dgw_info	   *myp;
dgw_object	   *obj;
register dgw_paths *thispath;
SP_DIR		   *dd;
{
	sp_result *out_list;
	int	   row;
	errstat	   err, retval;

	if ((err = lookup_entries(dd, &out_list)) != STD_OK) {
		dbprint(("dg_walk: setlookup failed (%s)\n", err_why(err)));
		return err;
	}

	/* Insert all entries that are directories */
	retval = STD_OK;
	for (row = 0; row < dd->dd_nrows; row++) {
		sp_result  *sr;
		capability  dcap;

		sr = &out_list[row];
		if (sr->sr_status != STD_OK) {
			/* lookup for this entry failed; maybe it was deleted
			 * between the sp_list and the sp_setlookup.
			 */
#ifdef DG_DEBUG
			char pathbuf[512];

			getpath(pathbuf, sizeof(pathbuf) - 1, thispath);
			printf("dirgraph: error looking up %s/%s (%s)\n",
				pathbuf, dd->dd_rows[row].d_name,
				err_why((errstat) sr->sr_status));
#endif
			continue;
		}

		if ((retval != STD_OK) ||
		    (myp->params.servers &&
		     !one_of(&sr->sr_capset, myp->params.servers)))
		{
			/* Previous insert failed, or port not marked as
			 * belonging to a directory server.
			 */
			cs_free(&sr->sr_capset);
			continue;
		}

		/* Check info string to make sure it's really a directory */
		if ((cs_to_cap(&sr->sr_capset, &dcap) == STD_OK) &&
		    _is_am_dir(&dcap))
		{
			/* Ok, found a dir. Now insert it */
			if (!insert(myp, &dcap, dd->dd_rows[row].d_name,
				    thispath, obj))
			{
				retval = STD_NOMEM;
			}
		}
		cs_free(&sr->sr_capset);
	}

	if (out_list != NULL) free(out_list);
	return retval;
}


static int
do_delve(myp,obj)
	struct dgw_info		*myp;
	dgw_object		*obj;
{
	register dgw_paths	*thispath;
	SP_DIR			*dd;
	capset			cset;
	errstat			err1;

	/* Try to get and see the contents */
	for(thispath = obj->paths ; thispath ; thispath = thispath->next){
		if ( thispath->done ) continue ;
		thispath->done = 1;
		if ( !cs_singleton(&cset,&thispath->cap) ) return 0;
		err1 = sp_list(&cset, &dd);
		switch(err1) {
		case SP_UNREACH:
			obj->flag |= DGW__IGNORE;
			cs_free(&cset);
			/* Don't bother to try anything else */
			return 1;
		case STD_OK:
			break;
		case STD_NOMEM:
			return 0;
		default:
			cs_free(&cset);
			continue;
		}

		err1 = add_dir_entries(myp, obj, thispath, dd);
		cs_free(&cset);
		sp_closedir(dd);
		if (err1 != STD_OK) return 0;
	}
	return 1 ;
}

static int
port_overlap(p, q)
	register capset *p, *q;
{
	register suite *a, *b;
	register int i, j;
	
	for (i = p->cs_final, a = p->cs_suite; --i >= 0; ++a) {
		if (!a->s_current)
			continue;
		for (j = q->cs_final, b = q->cs_suite; --j >= 0; ++b) {
			if (!b->s_current)
				continue;
			if ( PORTCMP(&a->s_object.cap_port,&b->s_object.cap_port )) {
				return 1;
			}
		}
	}
	return 0;
}


static int
one_of(p,q)
	capset		*p;
	capset		**q;
{
	register capset	**a;
	for ( a = q ; *a ; a++ ) {
		if ( port_overlap(p,*a) ) return 1 ;
	}
	return 0 ;
}

/* The hash routines */
static int
cap_eq(a, b)
	register capability *a, *b;
{
	register private *p, *q;
	rights_bits r;
	
	if (!PORTCMP(&a->cap_port, &b->cap_port))
		return 0;
	p = &a->cap_priv;
	q = &b->cap_priv;
	if (prv_number(p) != prv_number(q))
		return 0;
	if (p->prv_rights == q->prv_rights)
		return PORTCMP(&p->prv_random, &q->prv_random);
	if (p->prv_rights == ALL_RIGHTS)
		return prv_decode(q, &r, &p->prv_random) == 0;
	if (q->prv_rights == ALL_RIGHTS)
		return prv_decode(p, &r, &q->prv_random) == 0;
	/* Incomparable -- assume they are equal */
	return 2;
}

/* The rest of these hash routines were originally coded by Mike Condict */

/* Returns index of obj in the skip table, or if not found, returns
 * index of unused slot where it may be inserted.  It is an error for the
 * table to be full (and stupid for it to be more than 80% full).
 */
static int
obj_find(skip, cap)
	struct obj_table *skip;
	capability *cap;
{
	register int i,
		     num_tries = 0,
		     size = skip->size;
	register int	jump_factor ;
	int		h;
	objnum		o_num ;
	unsigned char	*cp = (unsigned char *)&cap->cap_port;

	o_num = prv_number(&cap->cap_priv);
	jump_factor = 1 + (o_num) % (size - 1);

	h = (int) (cp[0] * 1051 +	/* Hash the port and objnum */
		   cp[sizeof(port) >> 1] * 521 +
		   cp[sizeof(port) - 1] * 31 +
		   o_num) % skip->size;

	/* Avoid cycles that obviously are not going to examine the whole
	 * table:
	 */
	if (size % jump_factor == 0)
		jump_factor++;

	dbprint(("obj_find: --- SIZE %d ---- o_num = %d, h = %d, jump = %d\n",
		 size, o_num, h, jump_factor));

	/* Attempt to find obj or free slot using ever-increasing jumps: */
	for (i = h; num_tries < MAX_TRIES;
				    i = (i + jump_factor * ++num_tries) % size)
		if (UNUSED(skip, i) || OBJ_EQ(skip, i, cap))
			break;
	if (num_tries < MAX_TRIES) {
		dbprint(("obj_find(%d) -> %d (%s)\n",
			 num_tries, i, UNUSED(skip, i) ? "free" : "in use"));
		return i;
	}

	if (skip->num_entries >= size) {
		dbprint(("obj_find: ***** num_entries (%d) >= size (%d)\n",
			 skip->num_entries, size));
		return -1;	/* Table full ! */
	}

	/* As last resort, find obj or free slot by scanning forward from
	 * original hashed position:
	 */
	for (i = (h + 1) % size; i != h; i = (i + 1) % size)
		if (UNUSED(skip, i) || OBJ_EQ(skip, i, cap))
			break;
	if (i == h) {
		dbprint(("obj_find: ***** table full (#entries=%d; size=%d)\n",
			 skip->num_entries, size));
		return -1;	/* Table full ! */
	}
	dbprint(("obj_find(%d) -> %d (%s)\n",
		 num_tries, i, UNUSED(skip, i) ? "free" : "in use"));
	return i;
}

/* Allocate a hash table to keep track of objects already processed: */
static struct obj_table *
obj_table_alloc(size)
	int size;
{
	struct obj_table *skip = (struct obj_table *)
					malloc(sizeof(struct obj_table));
	if (skip == 0)
		return 0;
	skip->size = size;
	skip->num_entries = 0;
	skip->objs = (dgw_object **)
			calloc(skip->size, sizeof(dgw_object *));
	if (skip->objs == (dgw_object **)0) {
		free((char *)skip);
		return 0;
	}
	return skip;
}

/* Insert obj in table pointed to by *hashtable, if not already there.  If table
 * needs to be reallocated, change hashtable->objs to point to the new table and free
 * the old table.  Return flag indicating whether or not entry was already in
 * table:
 */
static dgw_object *
findobj(myp,cap,new)
	struct dgw_info		*myp;
	capability		*cap;
	int			*new;
{
    struct obj_table		*skip;
    register int		i, slot;
    register dgw_object		*obj;

    skip = myp->hasharray;
    slot  = obj_find(skip, cap);
    if (slot < 0 || slot >= skip->size)
	return (dgw_object *)0;	/* Huh? */

    if (UNUSED(skip, slot)) {
	*new = 1;	/* Was not already in table */
	if (skip->num_entries > skip->size * 3 / 4) {
	    /* Reallocate the hash table twice as large: */
	    struct obj_table *new_skip =
				    obj_table_alloc(2 * (1 + skip->size) - 1);
	    if (new_skip == 0)
		    return (dgw_object *)0;
	    for (i = 0; i < skip->size; i++) {
		obj = skip->objs[i];
		if ( obj ) {
		    slot = obj_find(new_skip,obj->cap);
		    if (slot < 0 || slot >= new_skip->size) {
			free((char *)new_skip->objs);
			free((char *)new_skip);
			return (dgw_object *)0;	/* Huh? */
		    }
		    new_skip->objs[slot] = obj;
		    ++new_skip->num_entries;
		}
	    }
	    free((char *)skip->objs);
	    *skip = *new_skip;
	    free((char *)new_skip);
	    slot = obj_find(skip, cap);
	    if (slot < 0 || slot >= skip->size)
		return (dgw_object *)0;	/* Huh? */
	}
	obj = getsmem(dgw_object);
	if (!obj) return (dgw_object *)0;
	obj->flag = 0;
	skip->objs[slot] = obj;
	++skip->num_entries;
    } else {
	*new = 0;	/* Was already in table */
	return skip->objs[slot] ;
    }
    return obj;
}
/* End of Condict derived code */

/* The routine used to visit all objects in such a way that access
   seems to be depth_first from the head
*/
static int
depth_first(myp,obj,proc)
	struct dgw_info		*myp;
	dgw_object		*obj;
	int			(*proc)();
{
	dgw_dirlist		*subdir;
	int			retval;

	obj->flag |= DGW__DONE ;
	if ( obj->flag&DGW__IGNORE ) return DGW_OK;
	retval = (*proc)(obj->paths);
	if ( retval == DGW_QUIT ) return DGW_QUIT ;
	for ( subdir = obj->subdirs ; subdir ; subdir = subdir->next ) {
		if ( !(subdir->dir->flag & DGW__DONE) ) {
			if ( depth_first(myp,subdir->dir,proc) == DGW_QUIT ) {
				return DGW_QUIT ;
			}
		}
	}
	return DGW_OK;
}

static int
visit_objects(myp,proc)
	struct dgw_info		*myp;
	int			(*proc)();
{
	register int		i;
	register int		size;
	dgw_object		**all;
	dgw_object		*obj;
	dgw_dirlist		*cur;

	size = myp->hasharray->size;
	all = myp->hasharray->objs;
	for (i = 0; i < size; i++) {
		obj = all[i];
		if ( obj ) obj->flag &= ~DGW__DONE;/* Clear the done flags */
	}
	for ( cur = myp->head ; cur ; cur = cur->next) {
		if ( depth_first(myp,cur->dir,proc) == DGW_QUIT ) return DGW_QUIT;
	}
	return DGW_OK;
}

/* This is an expansion routine for packages too lazy to do
   it themselves, or not willing to learn about the sp_ interface */

/* The following two routines use the two static variables declared
   here to keep track of where they are in the string
*/

static char	*rec_pathp;
static int	rec_nleft;

static void
printnext(path)
	dgw_paths	*path;
{
	register char	*entry;
	int		len;

	if ( !path ) return ;
	printnext(path->dotdot) ;
	entry = path->entry;
	/* We collapse double // */
	if ( !(entry[0] == '/' && entry[1] == 0) ) {
		if ( rec_nleft>0) strncpy(rec_pathp,entry,rec_nleft);
		len = strlen(entry);
		rec_nleft -= len;
		rec_pathp += len;
	}
	if (rec_nleft>0) *rec_pathp++ = '/';
	*rec_pathp = 0;
	rec_nleft--;
}

/* Main entry to routine printing a path */
/* The return value indicates the length of the string needed */
static int
getpath(buf,n,path)
	char		*buf;
	int		n;
	dgw_paths	*path;
{
	rec_pathp = buf; rec_nleft = n ;
	printnext(path) ;
	return n-rec_nleft ;
}

#define	DGW_PATHSZ	512
errstat
dgwexpand(path,proc)
	dgw_paths	*path;
	void		(*proc)();
{
	errstat			err1;
	SP_DIR			*dd;
	int			entry_count;
	capset			cset;
	register char		*pathbuf;
	register char		*entryp;
	int			pathsize;
	int			baselen;
	sp_result		*out_list;

	if ( !cs_singleton(&cset,&path->cap) ) {
		return STD_NOMEM ;
	}
	err1 = sp_list(&cset, &dd);
	if ( err1 != STD_OK ) {
		cs_free(&cset);
		return err1 ;
	}
	pathsize = 2*DGW_PATHSZ;
	pathbuf = (char *) malloc((size_t) pathsize);
	if ( !pathbuf ) {
		cs_free(&cset);
		sp_closedir(dd);
		return STD_NOMEM ;
	}
	baselen = getpath(pathbuf,pathsize,path);
	if ( baselen>DGW_PATHSZ ) {
		pathsize = baselen+DGW_PATHSZ;
		free(pathbuf);
		pathbuf = (char *) malloc((size_t) pathsize);
		if ( !pathbuf ) {
			cs_free(&cset);
			sp_closedir(dd);
			return STD_NOMEM ;
		}
		getpath(pathbuf,pathsize,path);
	}
	entryp = pathbuf+baselen;

	/* Lookup all entries using sp_setlookup. */
	if ((err1 = lookup_entries(dd, &out_list)) != STD_OK) {
		dbprint(("dg_walk: setlookup failed (%s)\n", err_why(err1)));
		sp_closedir(dd);
		return err1;
	}

	for( entry_count = 0 ; entry_count<dd->dd_nrows ; entry_count++){
		struct sp_direct *entry;
		register capset	 *child_cset;

		entry = &dd->dd_rows[entry_count] ;
		if ( out_list[entry_count].sr_status == STD_OK )
			child_cset = &out_list[entry_count].sr_capset;
		else
			child_cset = (capset *)NULL;
		strncpy(entryp,entry->d_name,pathsize-baselen);
		/* get parents name in here somehow */
		(*proc)(pathbuf,child_cset,&cset,entryp);
		if ( child_cset ) cs_free(child_cset) ;
	}

	sp_closedir(dd);
	cs_free(&cset);
	free(pathbuf);
	if (out_list != NULL) free(out_list);
	return STD_OK;
}

/* A small memory allocation package.
   Getting all these small memory segments in small malloc's
   would create an enormous overhead.
   The existance of separate lists serves one purpose:
	- to be able to separate memory freeable during the walk
	    from memory stable throughout the walk
	- to create a list of array's of object pointers
*/

static char *
getmem(l,n)
	struct memhead	*l;
	unsigned	n;
{
	register struct	memlist *cur ;
	register char		*retval;
	unsigned		touse;
	unsigned		toget;
	
	
	toget = MEMRND(n);
	cur = l->last ;
	if ( cur && cur->size-cur->used>=toget ) { /* It fits */
		retval = (char *)cur+ sizeof(*cur) + cur->used ; ;
		cur->used += toget ;
		return retval ;
	}
	/* No fit, try to find more */
	touse = toget + sizeof(struct memlist) ;
	if ( touse<MEMINCR ) {
		touse = MEMINCR ;
	} else {
		touse = MEMRND(touse);
	}
	retval = (char *) malloc((size_t) touse);
	if ( retval == NULL ) return NULL ;
	cur = (struct memlist *)retval ;
	cur->size = touse-sizeof(struct memlist)-toget ;
	cur->used = toget ;
	if ( l->last ) {
		l->last->next = (struct memlist *)retval ;
	} else {
		l->first = (struct memlist *)retval ;
	}
	l->last = (struct memlist *)retval ;
	retval += sizeof(struct memlist) ;
	cur->next = (struct memlist *)NULL;
	return retval ;
}

static void
freemem(l)
	struct memhead	*l;
{
	register struct memlist	*todo,*dofree;

	for ( todo = l->first ; todo ; todo = dofree ) {
		dofree = todo->next;
		free((char *)todo);
	}
	l->first = (struct memlist *)NULL;
	l->last = (struct memlist *)NULL;
}

/* Free all memory used by the walk */
static void
dgw_free(myp)
	struct dgw_info		*myp;
{
	freemem(&myp->dofirst) ;
	freemem(&myp->doafter) ;
	freemem(&myp->statmem) ;
	free(myp->hasharray->objs);
	free(myp->hasharray);
}
