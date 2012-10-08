/*	@(#)malloc.c	1.5	96/02/27 11:11:38 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	MALLOC/FREE
**
**  Malloc dynamically allocates a piece of memory of the requested size and
**  returns a pointer to it.  If the size is 0 or the memory could not be found
**  malloc returns 0.  The memory is guaranteed to be aligned to worst case
**  structure alignment.  No guarantees are made about what the memory contains
**  immediately after a free operation.
**  This is a linear malloc with very high performance.  It uses _getblk() to
**  get more physical memory.  It is based on the ack malloc but does not
**  assume that the next piece of requested memory is contiguous with the
**  last piece it got.
**
**  There are some compile time flags which you can set:
**	NON_PRE_EMPTIVE -
**		      set this is you have multi-threaded processes with
**		      non-pre-emptive sharing.
**	SEG_MERGE -   set this if the system can cope with the merging
**		      of segments.
**	DEBUG       - set this if you want to use the debug facilities.
**
**  The administration is as follows:
**
**   When physical memory must be allocated it is obtained as a segment using
**   _getblk().  Each segment has a header containing its size and a pointer
**   to the next segment header.  The linked list of segments is kept in
**   `decreasing address' order.
**
**   For each block of memory allocated by malloc there is a small header just
**   before the block which holds some pointers and the actual size of the
**   block (which may be slightly larger than the requested size since we
**   allocate in multiples of the alignment size).
**
**   There is an array of doubly linked freelists.  Freelist[i] contains free
**   blocks in the size range
**	[2^(i+log2(ALIGNMENT)), 2^(i+log2(ALIGNMENT)+1) - 1].
**   The value of i is the 'class' of a request.  The last class holds blocks
**   larger than 2^(i+log2(MIN_SIZE)).  The link pointers are in the block
**   headers.  
*/

#ifdef __MAL_DEBUG
/* In this case, the library contains a malloc wrapper that performs
 * additional consistency checks.  We define _malloc_real rather than
 * malloc, etc., to rule out direct calls to the primitives in this file.
 */
#define malloc  _malloc_real
#define calloc  _calloc_real
#define free    _free_real
#define realloc _realloc_real
#undef  __MAL_DEBUG
#endif

#include <stdlib.h>
#include <string.h>
#include "malloctune.h"	/* contains tuning parameters */

#ifndef NON_PRE_EMPTIVE

#include "amoeba.h"
#include "module/mutex.h"

static mutex	Lock;

#else
/*
** non pre-emptive scheduling with multiple threads or
** pre-emptive scheduling with single threads
*/

#define	mu_lock(x)
#define	mu_unlock(x)

#endif


/* Macros and Structures for Administration */
/********************************************/

/* Round up 'n' to a multiple of 'a', where 'a' is a power of 2 */
#define	ALIGN(n, a)	(((n) + (a) - 1) & ~((a) - 1))

/* Segment Header - must be aligned. */

typedef unsigned int	Blk_size;
typedef	struct Seg_hdr	Seg_hdr;
typedef struct Blk_hdr	Blk_hdr;

struct Seg_hdr
{
    Seg_hdr *	s_next;
    Seg_hdr *	s_prev;
    Blk_size	s_size;		/* size of segment (INcluding this header) */
};

#define	NULL_SHP		((Seg_hdr *) 0)

/*
**  Malloc block header.
** b_free_next:
** b_free_prev:	Pointers to next and previous blocks on free list of blocks
**		of the same class.  If the block is not free then b_free_next
**		has the value "IN_USE".  IN_USE should be defined as an illegal
**		pointer value other than the null pointer.
** b_phys_next:
** b_phys_prev:	Pointers to physically adjacent malloced blocks.  These are
**		null at the beginning and end of a segment!  These are used
**		to merge blocks when a block is freed.
** b_class:	The free list class which this block belongs to.
** b_size:	Size of the block in bytes, EXcluding the space taken by the
**		header.  The size of the header is aligned so that the user
**		block following it is correctly aligned.
*/

struct Blk_hdr
{
    Blk_hdr *	b_free_next;
    Blk_hdr *	b_free_prev;
    Blk_hdr *	b_phys_next;
    Blk_hdr *	b_phys_prev;
    Blk_hdr **	b_class;
    Blk_size	b_size;
};

#define	NULL_BP			((Blk_hdr *) 0)

#define	BLOCK_OF_MALLOC_HDR(x)	((char *) (x) + BLK_HDR_SIZE)
#define	BLK_HDR_SIZE		ALIGN(sizeof (Blk_hdr), ALIGNMENT)
#define	SEG_HDR_SIZE		ALIGN(sizeof (Seg_hdr), ALIGNMENT)

/*
** The following is used to mark a block as used and must be an
** illegal pointer other than the null pointer.
*/
#define IN_USE          ((Blk_hdr *) 1)

#define	BLK_IS_FREE(x)		((x)->b_free_next != IN_USE)
#define	MARK_IN_USE(x)		((x)->b_free_next = IN_USE)

/*
** The minimum size a piece of memory needs to be to hold a malloc block
** header and its block
*/
#define	MIN_BLKSZ		(ALIGNMENT + BLK_HDR_SIZE)

/* Global Variables */
/********************/

/* The free list heads for each class */
static	Blk_hdr *	Freelist[NUM_CLASSES];

/* head of list of allocated segments */
#if defined(DEBUG) || defined(SEG_MERGE)
static	Seg_hdr *	Seglist;
#endif

/* functions used internally by malloc/free */
static Blk_hdr *	add_new_seg();
static void		add_to_freelist();
static void		do_free();
static void		free_excess();
static Blk_hdr *	getfreeblk();
static Blk_hdr **	get_class();
static void		merge();
static void		remove_from_freelist();

#ifdef SEG_MERGE
static void		adjacent();
#endif


_VOIDSTAR	/* "char *" or "void *" depending on compiler */
malloc(n)
size_t n;
{
    register Blk_size	size;	/* size of the request */
    register Blk_hdr *	bhp;	/* block header pointer */

/*
** Need to stop other threads manipulating list pointers if current thread
** is pre-empted in the middle of manipulating list pointers
*/
    mu_lock(&Lock);
/*
** Make the size a multiple of ALIGNMENT.  (This is also the minimum size!)
** If size was illegal or we couldn't get the memory we return.
*/
    size = ALIGN(n, ALIGNMENT);
    if (size < ALIGNMENT || (bhp = getfreeblk(size)) == NULL_BP)
    {
	mu_unlock(&Lock);
	return (_VOIDSTAR) 0;
    }

/*
** Now we have a free block big enough for our needs.  If there is a leftover
** piece big enough for at least a  minimum sized block plus its header we
** give it back to the free list.
*/
    free_excess(bhp, size + BLK_HDR_SIZE);

    MARK_IN_USE(bhp);

    mu_unlock(&Lock);
    
    return (_VOIDSTAR) BLOCK_OF_MALLOC_HDR(bhp);
}


/*
**  FREE
**    Return a block allocated by malloc to the free list, merging it with
**    other free blocks where possible to make the largest possible free block.
*/

void
free(addr)
_VOIDSTAR addr;
{
    if (addr == 0)	/* just in case someone is naughty */
	return;
    mu_lock(&Lock);
    do_free((char *) addr);
    mu_unlock(&Lock);
}


/*
**  DO_FREE
**    Called by those parts that need to do a free even though the Lock is set
*/

static void
do_free(addr)
char *	addr;
{
    register Blk_hdr *	bp;	/* block to free */
    register Blk_hdr *	adj;	/* adjacent block */

    bp = (Blk_hdr *) (addr - BLK_HDR_SIZE);
#ifdef DEBUG
    if (BLK_IS_FREE(bp))	/* free consistency check */
	return;
#endif
    add_to_freelist(bp);

/* See if we can merge with adjacent blocks */
    if ((adj = bp->b_phys_next) != NULL_BP && BLK_IS_FREE(adj))
	merge(bp, adj);
    if ((adj = bp->b_phys_prev) != NULL_BP && BLK_IS_FREE(adj))
	merge(adj, bp);
}


/*
**  REALLOC
**	Try to increase the size of the previously allocated block pointed to
**	by 'addr'.  If there is space free just behind it then use that else
**	allocate a new block, copy the contents of the original block to the
**	new block and then free the old block.
*/

_VOIDSTAR
realloc(addr, newsize)
_VOIDSTAR	addr;
size_t		newsize;
{
    register Blk_size	size;
    register Blk_hdr *	bhp;	/* pointer to old block to be realloced */
    register Blk_hdr *	q;	/* pointer to block adjacent to bhp */
    register _VOIDSTAR	newmem;	/* pointer to newly malloced memory */
    size_t		oldsz;	/* size of block 'addr' */

    if (addr == 0)
	return malloc(newsize);
    
/* Make new size multiple of alignment and make sure it is valid */
    if ((size = ALIGN(newsize, ALIGNMENT)) <= 0)
    {
	free(addr);
	return (_VOIDSTAR) 0;
    }

    mu_lock(&Lock);
    bhp = (Blk_hdr *) (((char *)addr) - BLK_HDR_SIZE);

/* Is the new size smaller than the old or unchanged? */
    if (size <= (oldsz = bhp->b_size))
    {
	free_excess(bhp, size + BLK_HDR_SIZE);
	mu_unlock(&Lock);
	return addr;
    }

/*
** Otherwise the size got bigger and we have to do serious work.
** First we look to see if the adjacent physical block is free and has enough
** room to hold the size increase; if so we merge our block with it.
*/
    if ((q = bhp->b_phys_next) != NULL_BP && BLK_IS_FREE(q) &&
			    size <= bhp->b_size + q->b_size + BLK_HDR_SIZE)
    {
	if ((bhp->b_phys_next = q->b_phys_next) != NULL_BP)
	    bhp->b_phys_next->b_phys_prev = bhp;
	bhp->b_size += q->b_size + BLK_HDR_SIZE;
	bhp->b_class = get_class(bhp->b_size);
	remove_from_freelist(q);
	free_excess(bhp, size + BLK_HDR_SIZE);
	mu_unlock(&Lock);
	return (_VOIDSTAR) BLOCK_OF_MALLOC_HDR(bhp);
    }

/*
** Couldn't merge this block with an adjacent free block so we must
** malloc a new block, copy the old data to the new and free the old.
*/
    mu_unlock(&Lock);
    if ((newmem = malloc(newsize)) != 0)
	(void) memmove(newmem, addr, oldsz);
    free(addr);
    return newmem;
}


/* SUPPORT ROUTINES */
/********************/

/*
**  GETFREEBLK
**    Try to find a free block on the free list.  If there is one, unlink it
**    and return it.  Otherwise allocate a new segment and return that.
*/

static Blk_hdr *
getfreeblk(size)
register Blk_size	size;
{
    extern char *	_getblk();

    register Blk_hdr **	class;	/* free list class of this request */
    register Blk_hdr **	flp;	/* free list pointer */
    register Blk_hdr *	bp;	/* pointer to found block */

    class = get_class(size);

    /* See if there is a free block large enough */
    for (flp = class + 1;
	    *flp == NULL_BP && flp < &Freelist[NUM_CLASSES - 2];
		flp++)
	/* do nothing */ ;
    
    if (*flp == NULL_BP) /* nothing on free lists */
    {
	/* Search in the blocks in the real class, maybe one is big enough */
	for (bp = *class; bp != NULL_BP && bp->b_size < size;
							bp = bp->b_free_next)
	    /* do nothing */ ;

	if (bp == NULL_BP) /* nothing free in its real class either */
	{
	    register Seg_hdr *	shp;
	    register Blk_size	blksz;

	    blksz = ALIGN(size + 2 * BLK_HDR_SIZE + SEG_HDR_SIZE, SEGSIZE);
	    if ((shp = (Seg_hdr *) _getblk(blksz)) == NULL_SHP)
		return NULL_BP;
	    shp->s_size = blksz;
	    return add_new_seg(shp);
	}
    }
    else
	bp = *flp;	/* take first block on free list */

    remove_from_freelist(bp);
    return bp;
}


/*
**  FREE_EXCESS
**    Takes the piece of memory remaining in the given block, after size bytes
**    have been removed and puts it on the free list.
*/

static void
free_excess(bp, size)
register Blk_hdr *	bp;	/* hdr of block which is bigger than required */
Blk_size		size;	/* new size of bp, INclusive BLK_HDR_SIZE */
{
    register Blk_hdr *	new;		/* newly freed leftover */
    register Blk_hdr *	phys_next;	/* next physical block after bp */

/* Make sure there is enough space to make a minimum sized free block */
    if (bp->b_size <= size + MIN_BLKSZ)
	return;

    new = (Blk_hdr *) ((char *) bp + size);
    phys_next = bp->b_phys_next;

/* fill in the new header sizes */
    new->b_size = bp->b_size - size;
    new->b_class = get_class(new->b_size);
    bp->b_size = size - BLK_HDR_SIZE;
    bp->b_class = get_class(bp->b_size);

/* adjust the physical links */
    new->b_phys_prev = bp;
    if ((new->b_phys_next = phys_next) != NULL_BP)
	phys_next->b_phys_prev = new;
    bp->b_phys_next = new;

/* link `new' into the appropriate free list */
    add_to_freelist(new);

/* See if we can merge with adjacent blocks
** This will normally not occur but is here to make realloc work properly
** in the case when someone reduces the size of a block
*/
    if ((phys_next = new->b_phys_next) != NULL_BP && BLK_IS_FREE(phys_next))
	merge(new, phys_next);
}


/*
**  MERGE
**    Takes two blocks on the free list and merges them and puts the resultant
**    block on the correct free list.  Assumes the blocks really are physically
**    adjacent and that the address of 'b1' < address of 'b2'.
**    Caller must ensure that parameters aren't null pointers.
*/

static void
merge(b1, b2)
register Blk_hdr *	b1;
register Blk_hdr *	b2;
{
    register Blk_hdr **	new_class;

/* Don't merge with dummy block on end of segment */
    if (b2->b_size == 0)
	return;

/* Adjust sizes and links */
    b1->b_size += b2->b_size + BLK_HDR_SIZE;
    if ((b1->b_phys_next = b2->b_phys_next) != NULL_BP)
	b1->b_phys_next->b_phys_prev = b1;

/* Take b2 block off free list.  It has merged into b1 */
    remove_from_freelist(b2);

    new_class = get_class(b1->b_size);
    if (new_class != b1->b_class)	/* b1 has changed class! */
    {
	remove_from_freelist(b1);
	b1->b_class = new_class;
	add_to_freelist(b1);
    }
}


/*
**  GET_CLASS
**    Now find the free list class that 'size' bytes belongs in.
**    This is effectively log2(size) - log2(ALIGNSIZE).
**    This routine gets called a lot so it must be as fast as possible.
*/

#define	SKIP_JUMP	4
#define	SKIP_SHIFT	(ALIGN_SHIFT + SKIP_JUMP)
#define	SKIP		(1 << SKIP_SHIFT)

static Blk_hdr **
get_class(size)
register Blk_size	size;
{
    register Blk_hdr **	class;

    class = Freelist;

    /* Jump up SKIP_JUMP classes at a time */
    while (class+SKIP_JUMP < &Freelist[NUM_CLASSES - 2] && size > SKIP)
    {
	class += SKIP_JUMP;
	size >>= SKIP_JUMP;
    }

    for ( ; class < &Freelist[NUM_CLASSES - 2] && (size >>= 1) > ALIGNMENT;
	   class++)
	/* do nothing */ ;

    if (class > &Freelist[NUM_CLASSES - 2])
	class = &Freelist[NUM_CLASSES - 2];

    return class;
}


/*
**  REMOVE_FROM_FREELIST
**    Take item `bp' out of free list `class'
*/

static void
remove_from_freelist(bp)
Blk_hdr *	bp;
{
    register Blk_hdr *	next;
    register Blk_hdr *	prev;

    next = bp->b_free_next;
    prev = bp->b_free_prev;

    if (prev == NULL_BP)	/* at head of class */
	*bp->b_class = next;
    else
	prev->b_free_next = next;

    if (next)
	next->b_free_prev = prev;
}


/*
**  ADD_TO_FREELIST
**    Add item `bp' to free list b_class.  It is added at the head of the list.
*/

static void
add_to_freelist(bp)
Blk_hdr *	bp;
{
    bp->b_free_prev = NULL_BP;
    if ((bp->b_free_next = *bp->b_class) != NULL_BP)
	(*bp->b_class)->b_free_prev = bp;
    *bp->b_class = bp;
}


/*
**  ADD_NEW_SEG
**	Takes newly allocated segment and puts a dummy block header on the
**	end and a valid block header after the segment header and then
**	inserts it in the linked list of segments.  Thus a new segment is as
**	follows:
**		+--------+--------+-----------------------+--------+
**		| SEGHDR | BLKHDR |  space for user block | BLKHDR |
**		+--------+--------+-----------------------+--------+
**	If it is a kernel mode malloc it also tries to merge with physically
**	adjacent segments.
*/

static Blk_hdr *
add_new_seg(newseg)
register Seg_hdr *	newseg;
{
    register Blk_hdr *	head;

    { /* put tail into register */
	register Blk_hdr *	tail;	/* dummy block header at end of seg */

    /*
    ** First stick a dummy header on the end of the list, and make a block at
    ** the start which is the whole segment less segment header and dummy
    */
	head = (Blk_hdr *) ((char *) newseg + SEG_HDR_SIZE);
	tail = (Blk_hdr *) ((char *) newseg + newseg->s_size - BLK_HDR_SIZE);
	tail->b_size = 0;
	tail->b_class = Freelist;
	tail->b_phys_next = NULL_BP;
	tail->b_phys_prev = head;
	head->b_phys_next = tail;
	head->b_phys_prev = NULL_BP;
	head->b_size = newseg->s_size - 2 * BLK_HDR_SIZE - SEG_HDR_SIZE;
	head->b_class = get_class(head->b_size);
	MARK_IN_USE(head);
    } /* end of and tail in register */

#ifdef SEG_MERGE
/* We only use segment headers if there is the possibility to merge segments */

/* put into free list */
    { /* put shp and prevshp into registers */
	register Seg_hdr *	shp;
	register Seg_hdr *	prevshp;

	prevshp = NULL_SHP;
	for (shp = Seglist; shp && shp > newseg; shp = (prevshp = shp)->s_next)
	    /* do nothing */ ;
	if ((newseg->s_next = shp) != NULL_SHP)
	    shp->s_prev = newseg;
	if ((newseg->s_prev = prevshp) != NULL_SHP)
	    prevshp->s_next = newseg;
	else
	    Seglist = newseg;

/* merge new segment with adjacent segments */
	adjacent(shp, newseg);
	adjacent(newseg, prevshp);

    } /* end shp and prevshp in registers */

#endif /* SEG_MERGE */

    return head;
}


#ifdef SEG_MERGE

/*
**  ADJACENT
**	Merges two adjacent segments.  The first segment is assumed to have
**    a lower address than the second.
*/

static void
adjacent(seg1, seg2)
register Seg_hdr *	seg1;
register Seg_hdr *	seg2;
{
    register Blk_hdr *	dummy;	/* blk header at end of segment 1 */
    register Blk_hdr *	first2;	/* first block at start of segment 2 */

/* Check if either pointer is null.  Also see if segments are adjacent */
    if (!(seg1 && seg2) || (char *) seg1 + seg1->s_size != (char *) seg2)
	return;
/*
** The trick:  If seg1 and seg2 are adjacent then seg1's dummy Blk_hdr becomes
** the Blk_hdr of the area previously occupied by seg2's Seg_hdr.  Segment seg2
** disappears from the administration and is appended to seg1.  The links
** between the Blk_hdrs are properly made, and the newly created block is
** marked as free.
*/
    dummy = (Blk_hdr *) ((char *) seg1 + seg1->s_size - BLK_HDR_SIZE);
    first2 = (Blk_hdr *) ((char *) seg2 + SEG_HDR_SIZE);
    dummy->b_phys_next = first2;
    dummy->b_size = SEG_HDR_SIZE;
    first2->b_phys_prev = dummy;
    seg1->s_size += seg2->s_size;
    if ((seg1->s_prev = seg2->s_prev) != NULL_SHP)
	seg1->s_prev->s_next = seg1;
    else
	Seglist = seg1;
    do_free((char *) dummy + BLK_HDR_SIZE);
}
#endif /* SEG_MERGE */


#ifdef DEBUG

#include "stdio.h"
prstatus()
{
    register Seg_hdr *	shp = Seglist;
    register Blk_hdr *	bp;
    register int i;

    printf("### Malloc administration ###\n");
/* for each segment */
    for (shp = Seglist; shp != NULL_SHP; shp = shp->s_next)
    {
	printf("Segment: addr %lx size %d, prev %lx next %lx\n",
			    shp, shp->s_size, shp->s_prev, shp->s_next);
    /* for each block in the segment */
	for (bp = (Blk_hdr *) ((char *) shp + SEG_HDR_SIZE); bp != NULL_BP;
							bp = bp->b_phys_next)
	    prblock(bp);
    }
    printf("Freelist:\n");
    for (i = 0; i < NUM_CLASSES; i++)
    {
	printf("  Class %d:", i);
	for (bp = Freelist[i]; bp != NULL_BP; bp = bp->b_free_next)
	{
	    printf(" %lx", bp);
	    if (bp->b_free_next == bp)
	    {
		   printf(" ***");
		   break;
	    }
	}
	printf(" 0\n");
    }
    fflush(stdout);
}


prseglist()
{
    Seg_hdr *	shp;

    if (Seglist == 0)
	printf("Seglist = -|\n");
    else
	for (shp = Seglist; shp != 0; shp = shp->s_next)
	    printf("prev=%lx, next=%lx, size=%d\n",
				    shp->s_prev, shp->s_next, shp->s_size);
}


prblock(bp)
Blk_hdr *	bp;
{
    printf(" Block: bp %lx pn %lx pp %lx fn %lx fp %lx cl %d sz %d @ %lx\n",
	    bp, bp->b_phys_next, bp->b_phys_prev, bp->b_free_next,
	    bp->b_free_prev, bp->b_class - Freelist, bp->b_size,
	    (char *) bp + BLK_HDR_SIZE);
}

#endif /* DEBUG */
