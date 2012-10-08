/*	@(#)alloc.c	1.5	96/02/27 14:11:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * ALLOC.C
 *
 *	This is the resource allocation module.  It maintains free lists for
 *	specified resources in the bullet server.  A resource is specified
 *	by a resource identifier (see alloc.h for valid identifiers).
 *	The free lists for the resources are built at startup time so that
 *	they don't need to be maintained on the disk.  Thus the free list
 *	of disk blocks and inodes is constructed from the information in the
 *	inode table (which IS kept on disk).  They can be reconstructed at
 *	any time.
 *	For each resource a linked list of holes in the resource continuum
 *	is kept.  The head of the linked list is the Resource struct and for
 *	each hole there is a Hole struct.
 *
 *  Author: 	Annita Wilschut
 *  Rewritten: 	Greg Sharp  13-01-89
 *  Modified: 	Greg Sharp  25-10-90
 *		Made to be auto-configuring so that if it runs out of Hole
 *		structs it allocates more.
 */

#include "amoeba.h"
#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "alloc.h"
#include "stdlib.h"
#include "cache.h"
#include "bs_protos.h"
#include "preempt.h"

/*
 * Pool of hole structs.  It is allocated at startup time on the basis of
 * A_HOLES_PER_MEG holes per megabyte of disk + main memory.  It will grow
 * automatically as required but if it gets above A_HOLE_LIMIT holes per
 * megabyte warnings will appear.
 */
static Hole *	a_freeholes;

/*
 * Total number of holes malloced & limit on sane number of holes to
 * allocate
 */
static unsigned	a_total_holes;
static unsigned	a_hole_limit;

/* List of resources */
static Resource	a_resource[A_RESOURCES];


static Hole *
a_grow_hole_pool(num_holes)
unsigned int	num_holes;	/* # new hole structs to allocate */
{
    register Hole *	newmem;
    register int	i;

    newmem = (Hole *) calloc(num_holes, sizeof (Hole));

    if (newmem != (Hole *) 0)
    {
	for (i = 0; i < num_holes - 1; i++)
	    newmem[i].h_next = &newmem[i+1];
	newmem[num_holes - 1].h_next = (Hole *) 0;
	a_total_holes += num_holes;
    }
    if (a_total_holes > a_hole_limit)
	bwarn("excessive fragmentation\n");
    return newmem;
}


/*
 * A_GETSLOT
 *
 * Gets a Hole struct from the pool of free structs.
 * It panics if it runs out of slots since the mallocing of new structs
 * and cache compaction should prevent this from ever happening.
 */
static Hole *
a_getslot()
{
    register Hole *	ap;

    if (a_freeholes == (Hole *) 0 &&
	    (a_freeholes = a_grow_hole_pool(a_total_holes / 4)) == (Hole *) 0)
	bpanic("a_getslot: No free hole structs.\n");
    ap = a_freeholes;
    a_freeholes = a_freeholes->h_next;
    return ap;
}


/*
 * A_INIT
 *
 * Initializes the allocation module by defining all resources as
 * non-existent and putting all holes on the free list.
 * This routine can only be run while the bullet server is still
 * single-threaded.
 */
void
a_init(num_megs)
unsigned int	num_megs;	/* # Megabytes of resource in the system */
{
    int		i;

    /* Initialise resources to non-existent */
    for (i = 0; i < A_RESOURCES; i++)
    {
        a_resource[i].r_bottom = 0;
	a_resource[i].r_top = 0;
	a_resource[i].r_holelist = (Hole *) 0;
	a_resource[i].r_in_use = 0;
    }

    /* Initialize hole freelist */
    a_hole_limit = num_megs * A_HOLE_LIMIT;
    a_freeholes = a_grow_hole_pool(num_megs * A_HOLES_PER_MEG);
    if (a_freeholes == 0)
	bpanic("a_init: calloc failed\n");
    ENABLE_PREEMPTION;
}


/*
 * A_BEGIN
 *
 * Start a new resource with the entire resource free.
 * Returns -1 if resource was already "begun", there were no free Hole structs
 * or if top and bottom were invalid.  Otherwise it returns 0.
 */
int
a_begin(rid, bottom, top)
Res_id		rid;		/* resource identifier */
Res_addr	bottom;		/* address of first resource object */
Res_addr	top;		/* address of last resource object */
{
    register Hole *	ap;
    register Resource *	rp;

    /*
     * Can only start a resource that has a positive amount of resource,
     * has not been started already and if there are some free hole structs
     */
    rp = &a_resource[rid];
    DISABLE_PREEMPTION;
    if (top <= bottom || rp->r_in_use || a_freeholes == 0)
    {
	ENABLE_PREEMPTION;
	return -1;
    }

    /* Make an initial hole which is the entire resource */
    ap = a_freeholes;
    a_freeholes = a_freeholes->h_next;
    ap->h_next = 0;
    ap->h_size = top - bottom ;
    ap->h_addr = bottom;

    /* Fill in the resource header */
    rp->r_holelist = ap;
    rp->r_bottom = bottom;
    rp->r_top = top;
    rp->r_in_use = 1;
    ENABLE_PREEMPTION;
    return 0;
}


/*
 * A_USED
 *
 * Is the routine to call when setting up the free list from what
 * is already in use on the disk.  It is different from a_alloc since it
 * doesn't find a free patch and allocate it, but rather it must make the
 * free list correspond to what is already allocated.  It assumes that all
 * adjacent holes are merged.
 * An error is returned if any part of the area is used already.
 * The function returns 0 on success and -1 if any error is encountered.
 */
int
a_used(rid, addr, size)
Res_id			rid;	/* resource identifier */
register Res_addr	addr;	/* start address of used piece of resource */
register Res_size	size;	/* size of used piece of resource */
{
    register Hole *	nhp;	/* temp pointer to a hole */
    register Hole *	hp;	/* pointer to a hole of hhp */
    register Hole **	hpp;	/* pointer to current hole in resource */
    Resource *		rp;	/* head of resource free list */
    Res_addr		hpptop;	/* top end address of the hole */

    /*
     * If the resource is not in use or the address is not in the range of the
     * resource allocator then error.  Note that due to possible overflow of
     * addr+size a test is made for overflow or size <= 0.
     */
    rp = &a_resource[rid];
    DISABLE_PREEMPTION;
    if (!rp->r_in_use || addr < rp->r_bottom || size <= 0 ||
				    addr+size <= addr || addr+size > rp->r_top)
    {
	ENABLE_PREEMPTION;
	return -1;
    }

    /*
     * For each hole, if addr begins in the hole, split the hole and return.
     * It uses double indirection to handle the lack of back pointers in the
     * list.
     */
    for (hpp = &(rp->r_holelist); *hpp; hpp = &(*hpp)->h_next)
    {
	hp = *hpp;
	hpptop = hp->h_addr + hp->h_size;
	if (addr < hpptop && addr >= hp->h_addr)
	{
	    if (addr == hp->h_addr)	/* then resource is at start of hole */
	    {
		if (size < hp->h_size)	/* then remove start of hole */
		{
		    hp->h_addr = addr + size;
		    hp->h_size -= size;
		    ENABLE_PREEMPTION;
		    return 0;
		}
		if (size == (*hpp)->h_size) /* then resource is complete hole */
		{
		    *hpp = hp->h_next;
		    hp->h_next = a_freeholes;
		    a_freeholes = hp;
		    ENABLE_PREEMPTION;
		    return 0;
		}
		ENABLE_PREEMPTION;
		return -1;	/* size > hole size */
	    }
	    /* else addr > (*hpp)->addr */
	    if (addr+size < hpptop)	/* then in middle of hole */
	    {
		/* Split the hole into two parts */
		if ((nhp = a_getslot()) == 0)
		{
		    ENABLE_PREEMPTION;
		    return -1;
		}
		nhp->h_addr = hp->h_addr;
		nhp->h_size = addr - hp->h_addr;
		hp->h_addr = addr + size;
		hp->h_size = nhp->h_addr + hp->h_size - hp->h_addr;
		nhp->h_next = *hpp;
		*hpp = nhp;
		ENABLE_PREEMPTION;
		return 0;
	    }
	    if (addr+size == hpptop)	/* then at end of hole */
	    {
		hp->h_size -= size;
		ENABLE_PREEMPTION;
		return 0;
	    }
	    ENABLE_PREEMPTION;
	    return -1;
	}
    }
    ENABLE_PREEMPTION;
    return -1;	/* no hole contained addr so resource was already in use */
}


/*
 * A_ALLOC
 *
 * Allocates an area of the given size.  The address of this area
 * is returned through ``addr''.  It returns the address of the start of
 * the allocated area if it succeeds, or 0 if it fails.
 * NB: This means that 0 cannot be a valid address!
 */
Res_addr
a_alloc(rid, size)
Res_id		rid;	/* resource type */
Res_size	size;	/* amount of resource required */
{
    register Hole **	app;
    register Hole *	ap;
    register Res_addr	addr;
    Resource *		rp;

    rp = &a_resource[rid];
    DISABLE_PREEMPTION;
    if(!rp->r_in_use)
    {
	ENABLE_PREEMPTION;
	return 0;
    }

    for (app = &(rp->r_holelist), ap = *app;
			ap != 0; app = &(ap->h_next), ap = *app)
    {
	if (size < ap->h_size)	/* alloc the beginning of this hole */
	{
	    addr = ap->h_addr;
	    ap->h_addr += size;
	    ap->h_size -= size;
	    ENABLE_PREEMPTION;
	    return addr;
	}
	if (size == ap->h_size)	/* Remove this hole */
	{
	    addr = ap->h_addr;
	    *app = ap->h_next;
	    ap->h_next = a_freeholes;
	    a_freeholes = ap;
	    ENABLE_PREEMPTION;
	    return addr;
	}
    }
    ENABLE_PREEMPTION;
    return 0;
}


/*
 * A_FREE
 *
 * Returns to the free list the "size" units of resource "rid" starting
 * at "begin".
 * -1 is returned if any part of this area is free already.
 * 0 is returned if the operation completed successfully.
 */
int
a_free(rid, begin, size)
Res_id			rid;
register Res_addr	begin;	/* start address of block to be freed */
register Res_size	size;	/* size of block to be freed */
{
    register Hole **	app;
    register Hole *	ap;
    register Hole *	nap;	/* new address hole pointer */
    register Res_addr	end;
    Resource *		rp;

    if (size == 0)	/* nothing to free */
	return 0;

    /* Make sure the resource is active and the region to be freed is valid */
    end = begin + size;
    DISABLE_PREEMPTION;
    rp = &a_resource[rid];
    if (!rp->r_in_use || begin < rp->r_bottom || end < begin || end > rp->r_top)
    {
	ENABLE_PREEMPTION;
	return -1;
    }

    /* Find the place where the area should go */
    for (app = &(rp->r_holelist); *app; app = &(*app)->h_next)
    {
	ap = *app;
	if (begin < ap->h_addr)
	{
	    /* Area to be freed goes before current hole */
	    if (end < ap->h_addr)
	    {
		/*
		 * Area is in middle of used area before this hole.
		 * Make new hole.
		 */
		if ((nap = a_getslot()) == 0)
		{
		    ENABLE_PREEMPTION;
		    return -1;
		}
		nap->h_addr = begin;
		nap->h_size = size;
		nap->h_next = *app;
		*app = nap;
		ENABLE_PREEMPTION;
		return 0;
	    }
	    if (end == ap->h_addr)
	    {
		/* Add to the beginning of the current free area */
		ap->h_addr = begin;
		ap->h_size += size;
		ENABLE_PREEMPTION;
		return 0;
	    }
	    ENABLE_PREEMPTION;
	    return -1;	/* Overlaps free area pointed to by *app */
	}
	if (begin == ap->h_addr + ap->h_size)
	{
	    /*
	     * Area to be freed goes at the end of the current free area.
	     * Add it to the current free hole.
	     */
	    nap = ap->h_next;
	    if (nap == 0 || end < nap->h_addr)
	    {
		ap->h_size += size;
		ENABLE_PREEMPTION;
		return 0;
	    }
	    if (end == nap->h_addr)
	    {
		/*
		 * The current free area can now be merged with the next free
		 * area.
		 */
		ap->h_size += size + nap->h_size;
		ap->h_next = nap->h_next;
		nap->h_next = a_freeholes;
		a_freeholes = nap;
		ENABLE_PREEMPTION;
		return 0;
	    }
	    ENABLE_PREEMPTION;
	    return -1;	/* Overlaps free area pointed to by *app */
	}
	if (begin < ap->h_addr + ap->h_size)
	{
	    ENABLE_PREEMPTION;
	    return -1;	/* Overlaps free area pointed to by *app */
	}
    }

    /*
     *  We've come to the end of the free list.  Therefore the area to be
     *  freed belongs after the end of the current free list.
     *  So we just add it to the end of the free list.
     */
    if ((nap = a_getslot()) == 0)
    {
	ENABLE_PREEMPTION;
	return -1;
    }
    nap->h_addr = begin;
    nap->h_size = size;
    nap->h_next = 0;
    *app = nap;
    ENABLE_PREEMPTION;
    return 0;
}


/*
 * Count how many units in the resource are not in use
 */
Res_size
a_notused(rid)
Res_id	rid;
{
    Resource *		rp;
    register Hole *	ap;
    register Res_size	total;

    rp = &a_resource[rid];
    DISABLE_PREEMPTION;
    if (!rp->r_in_use)
    {
	ENABLE_PREEMPTION;
	return -1;
    }

    for (total = 0, ap = rp->r_holelist; ap; ap = ap->h_next)
	total += ap->h_size;

    ENABLE_PREEMPTION;
    return total;
}


/*
 * Count the number of holes in the resource.
 */
long
a_numholes(rid)
Res_id	rid;
{
    Resource *		rp;
    register Hole *	ap;
    register long	total;

    rp = &a_resource[rid];
    DISABLE_PREEMPTION;
    if (!rp->r_in_use)
    {
	ENABLE_PREEMPTION;
	return -1;
    }

    for (total = 0, ap = a_resource[rid].r_holelist; ap; ap = ap->h_next)
	total++;

    ENABLE_PREEMPTION;
    return total;
}


/*
 * Count the free Hole structs still in the pool
 */
long
a_numfreeholes()
{
    register Hole *	ap;
    register long	i;
    
    DISABLE_PREEMPTION;
    for (i = 0, ap = a_freeholes; ap; ap = ap->h_next)
	i++;
    ENABLE_PREEMPTION;
    return i;
}


int
a_high_fragmentation(rid)
Res_id	rid;
{
    return a_numholes(rid) > (a_total_holes / 2);
}


/*
 *  COM_FIRSTHOLE
 *
 *	Sets up the static variables for the com_nexthole() function so that
 *	one can single step through the holes of a resource to do compaction.
 *	Return in p and size the details of the first hole after address begin.
 *	Returns 1 if there are 2 or more holes in the resource and it
 *	succeeded in doing the set up.  Otherwise it returns 0.
 *	NB it is a good idea to lock out changes to the free list while you
 *	are traversing it.
 */

static Hole *	Cur_pos[A_RESOURCES];

int
com_firsthole(rid, p, size, addr)
Res_id		rid;
Res_addr *	p;
Res_size *	size;
Res_addr	addr;
{
    register Hole *      ap;
 
    DISABLE_PREEMPTION;
    /* Is resource active? */
    if (!a_resource[rid].r_in_use)
    {
	ENABLE_PREEMPTION;
        return -1;
    }
 
    /* No holes or only one hole? */
    if ((ap = a_resource[rid].r_holelist) == 0 || ap->h_next == 0)
    {
	ENABLE_PREEMPTION;
        return 0;
    }
    /* Find the first hole after "addr" */
    while (ap->h_addr < addr)
	if ((ap = ap->h_next) == (Hole *) 0)
	{
	    ENABLE_PREEMPTION;
	    return 0;
	}
    Cur_pos[rid] = ap;
    *p = ap->h_addr;
    *size = ap->h_size;
    ENABLE_PREEMPTION;
    return 1;
}


/*
 * COM_NEXTHOLE
 *
 *	Returns pointer and size for next hole in the given resource.  It keeps
 *	the place via static variables which are set up by com_firsthole() to
 *	start at the beginning.  Returns 1 if it succeeded and 0 if no holes
 *	were left.
 *	NB it is a good idea to lock out changes to the free list while you
 *	are traversing it.
 */
int
com_nexthole(rid, p, size)
Res_id  	rid;
Res_addr *	p; 
Res_size *	size; 
{ 
    register Hole *      ap;
 
    /* Is resource active? */
    DISABLE_PREEMPTION;
    if (!a_resource[rid].r_in_use)
    {
	ENABLE_PREEMPTION;
        return -1;
    }
    if ((ap = Cur_pos[rid]->h_next) == 0)
    {
	ENABLE_PREEMPTION;
	return 0;
    }
    Cur_pos[rid] = ap;
    *p = ap->h_addr;
    *size = ap->h_size;
    ENABLE_PREEMPTION;
    return 1;
}


#ifndef NDEBUG

/*
 * A_DUMP
 *
 * Prints the free list for a particular resource on stdout,
 * which should be the console of the bullet server host.
 */
void
a_dump(rid)
Res_id	rid;
{
    Hole *	ap;
    Resource *	rp;

    DISABLE_PREEMPTION;
    rp = &a_resource[rid];
    if (!rp->r_in_use)
	printf("a_dump: Free-list not initialised for RID %d\n", rid);
    else
    {
	printf("a_dump: Dumping free-list for RID %d\n", rid);
	for (ap = rp->r_holelist; ap; ap = ap->h_next)
	    printf("address: 0x%08x  to 0x%08x. size: 0x%x\n",
			ap->h_addr, ap->h_addr + ap->h_size, ap->h_size);
	printf("a_dump: end of free list for RID %d\n", rid);
    }
    ENABLE_PREEMPTION;
}
#endif /* NDEBUG */
