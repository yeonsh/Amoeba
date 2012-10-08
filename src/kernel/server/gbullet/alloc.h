/*	@(#)alloc.h	1.1	96/02/27 14:06:51 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ALLOC_H__
#define	__ALLOC_H__

/*
 *	ALLOC.H
 *
 *	This file contains the structs and constants used by the resource
 *	allocator.  For each resource it keeps a "struct Resource" which
 *	holds the largest and the least "address" of the resource and a
 *	pointer to free items of the resource.  It bunches the free objects
 *	and regards them as "holes" in the continuum of the resource.
 *	For each hole there is a "struct Hole" describing the start address
 *	of the hole, the size of the hole and a pointer to the next hole.
 *
 *	NB:	There is a restriction on addresses.  They must be positive
 *		integers since addresses are signed longs.
 *
 *	This is based on Annita Wilschut's original allocator.
 *
 *  Author:
 *	Greg Sharp 13-01-89
 */

typedef struct Hole	Hole;
typedef struct Resource	Resource;

typedef int		Res_id;		/* resource identifier */
typedef unsigned long	Res_addr;	/* resource address */
typedef unsigned long	Res_size;	/* resource size */

/*
 * The hole struct describes a piece of free resource in the range specified
 * by the resource header (described below)
 * h_addr:	the base "address" of the free resource
 * h_size:	the amount of free resource which begins at h_size.  The units
 *		are arbitrary and no unit conversions are possible so the
 *		user had better get them right!
 * h_next:	pointer to the next hole in the free list.
 */

struct Hole
{
    unsigned long	h_addr;	/* starting address of hole */
    unsigned long	h_size;	/* size of hole */
    Hole *		h_next;	/* link to next element in linked list */
};


/*
 * The resource struct is the head of the resource free list.
 * r_bottom:	the lowest "address" of the resource being managed
 * r_top:	the maximum "address" of the resource being managed
 * r_holelist:	pointer to list of unallocated resource (holes free between
 *		r_top and r_bottom.
 * r_in_use:	a boolean.  It is true if the resource struct is the head of
 *		a freelist that is in use.  It is false otherwise.
 */

struct Resource
{
    unsigned long	r_bottom;	/* lowest possible address */
    unsigned long	r_top;		/* highest possible address  + 1*/
    unsigned long	r_n_free;		/* # of free resources */
    Hole *		r_holelist;	/* pointer to list of holes */
    int			r_in_use;	/* is this resource freelist in use? */
};


/*
 * We allocate Hole structs on the basis of the number of units of resource
 * to be managed.  For memory (disk or cache) we reckon on an initial maximum
 * of A_HOLES_PER_MEG holes per Meg.  If it goes over A_HOLE_LIMIT Holes per
 * Meg then something is out of hand and warnings are printed.
 */
#define	A_HOLES_PER_MEG	6
#define	A_HOLE_LIMIT	10

/* Total number of resource types */
#define	A_RESOURCES	3

/* Valid resource identifiers */
#define	A_CACHE_MEM	0	/* file cache memory */
#define	A_DISKBLOCKS	1

/* External routines provided by allocation package */
Res_addr   a_alloc _ARGS((Res_id, Res_size));
Res_size   a_notused _ARGS((Res_id));
Res_size   a_nfree _ARGS((Res_id));
int	   a_high_fragmentation _ARGS((Res_id));
int	   a_used _ARGS((Res_id, Res_addr, Res_size));
int	   a_free _ARGS((Res_id, Res_addr, Res_size));
void	   a_init _ARGS((unsigned int));
long	   a_numfreeholes _ARGS((void));
long	   a_numholes _ARGS((Res_id));
int	   com_firsthole _ARGS((Res_id, Res_addr *, Res_size *, Res_addr));
int	   com_nexthole _ARGS((Res_id, Res_addr *, Res_size *));
#ifndef NDEBUG
void	   a_dump _ARGS((Res_id));
#endif /* NDEBUG */

#endif /* __ALLOC_H__ */
