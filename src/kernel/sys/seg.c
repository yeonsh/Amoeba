/*	@(#)seg.c	1.12	96/02/27 14:23:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "machdep.h"
#include "process/proc.h"
#ifdef lint /* Lint wants to know about the struct mapping */
#include "global.h"
#include "exception.h"
#include "kthread.h"
#endif
#include "seg.h"
#include "file.h"
#include "map.h"
#include "bullet/bullet.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "module/stdcmd.h"
#include "string.h"
#include "sys/proto.h"

#ifndef NOPROC
#include "monitor.h"
#endif

#define CAPCMP(x, y)	(memcmp((_VOIDSTAR)(x), \
				(_VOIDSTAR)(y), sizeof(capability))==0)

/* # segments for preliminary segment table */
#define	N_SEGS		10

static void merge_segment();


static struct segtab *	segtab;
static struct segtab *	freesegtab;
static struct segtab *	aaseg;
static vir_bytes	aaptr;
static vir_bytes	aalen;

static seg_init_level;	/* initialized at 0 */

#define SIL_NO_INIT	0
#define SIL_FIRST_INIT	1
#define SIL_FULL_INIT	2

static struct segtab **segtab_rev;	/* mapping segment number to segment */

extern port		segsvr_put_port;
static int		nseg;

/*-------------------------------------------------------------------
**
** Part 1: segtab_alloc and segtab_free.
**
** These routine manage the pool of segtab structs. These structs
** are used to store information about segments.
** The structures that are in use are linked in physical memory
** order, so it is easy to collapse free segments into one, or check
** whether a segment can grow.
**
**-------------------------------------------------------------------
*/
/*
** segtab_alloc - Return new segment structure inserted in linked
**		  list after 'prev'.
*/
static struct segtab *
segtab_alloc(prev)
register struct segtab *prev;
{
    register struct segtab *p;

    assert(prev != NILSEG);
    if ((p = freesegtab) == NILSEG)
    {
	DPRINTF(0, ("segtab_alloc: out of segtab entries\n"));
	return (struct segtab *) 0;
    }
    freesegtab = freesegtab->seg_next;
    p->seg_next = prev->seg_next;
    p->seg_prev = prev;
    if (p->seg_next != NILSEG)
	p->seg_next->seg_prev = p;
    prev->seg_next = p;
    p->seg_back = 0;
    return p;
}


/*
** segtab_free - remove a segment structure from list.
*/
static void
segtab_free(p)
register struct segtab *p;
{
    assert(p != NILSEG);
/* unlink it */
    if (p->seg_next != NILSEG)
	p->seg_next->seg_prev = p->seg_prev;
    assert(p->seg_prev != NILSEG);
    p->seg_prev->seg_next = p->seg_next;

/* mark it free and put it on front of free list */
    p->seg_type = ST_UNUSED;
    p->seg_count= 0;
    p->seg_next = freesegtab;
    p->seg_prev = 0;
    freesegtab = p;
}


/* Put p back on the freelist if add_memchunk fails */
static void
add_free(p)
struct segtab *	p;
{
    p->seg_count = 0;
    p->seg_next = freesegtab;
    freesegtab = p;
}


static struct segtab *
add_memchunk(start, size, type, count)
phys_clicks start;
phys_clicks size;
long type;
long count;
{
    register struct segtab *	p;
    register struct segtab *	sp;

    if ((p = freesegtab) == NILSEG)
	return p;
    freesegtab = freesegtab->seg_next;

    /* Fill in new segment */
    p->seg_begin = start;
    p->seg_size = size;
    p->seg_type = type;
    p->seg_count = count;
    p->seg_back = 0;

    /* Put it in the right place on the "in use list" */
    sp = segtab;
    if (sp == NILSEG)
    {
	/* First one on the list */
	p->seg_next = NILSEG;
	p->seg_prev = NILSEG;
	segtab = p;
    }
    else
    {
	while (start >= sp->seg_begin + sp->seg_size && sp->seg_next != NILSEG)
	    sp = sp->seg_next;
	if (start >= sp->seg_begin + sp->seg_size)
	{
	    /* New segment goes after sp */
	    assert(sp->seg_next == NILSEG);
	    p->seg_next = sp->seg_next;
	    p->seg_prev = sp;
	    sp->seg_next = p;
	}
	else
	{
	    /* New segment goes before sp */
	    if (sp->seg_begin < start + size)
	    {
		/* New segment overlaps the sp */
		add_free(p);
		return NILSEG;
	    }
	    p->seg_next = sp;
	    p->seg_prev = sp->seg_prev;
	    if (p->seg_prev != NILSEG)
		p->seg_prev->seg_next = p;
	    else
		segtab = p;
	    sp->seg_prev = p;
	}
    }
    return p;
}


/*-------------------------------------------------------------------
**
** Part 2: mem_findhole and mem_compact. These routines search the
** linked list of segtab entries for available memory. If none
** is available mem_compact is called to compact memory and merge
** all freespace into one hole.
**-------------------------------------------------------------------
*/
/*
** mem_findhole - find a hole of right size.
** 
** The algorithm isn't trivial, since we want to keep segments
** with original contents and segments with a capability around.
** So, we run through memory looking for a segment, but we also
** note the largest amount of contiguous freespace available.
** If we cannot find a free segment we try that.
*/
static struct segtab *
mem_findhole(size, direction)
vir_clicks size;
int direction;
{
    register struct segtab * p;
    register struct segtab * q;
    struct segtab * contp;	/* best contiguous free space pointer */
    struct segtab * curcontp;	/* current contiguous free space pointer */
    int	contsize;		/* best contiguous free space size */
    int	curcontsize;		/* current contiguous free space size */

    assert(size > 0);
    assert((direction & ~1) == 0);
    contsize = curcontsize = 0;
    contp = 0;
    if (direction == 1)
    {
	/* Allocate memory from the end of free memory */
	p = segtab;
	assert(p != 0);
	while (p->seg_next != NILSEG)
	    p = p->seg_next;
	
	for ( ; p != NILSEG; p = p->seg_prev)
	{
	    if (p->seg_count == 0) /* segment is not in use */
		curcontsize += p->seg_size;
	    else
		curcontsize = 0; /* segment is in use */

	    /* remember the largest contiguous piece */
	    if (curcontsize > contsize)
	    {
		contsize = curcontsize;
		contp = p;
	    }
	    if (p->seg_type == ST_UNUSED && p->seg_size >= size)
	    {
		/*
		** It fits. See whether we leave some space at the beginning.
		*/
		if (p->seg_size > size)
		{
		    q = segtab_alloc(p);
		    if (q == 0)
			return NILSEG;
		    q->seg_size = size;
		    p->seg_size -= size;
		    q->seg_begin = p->seg_begin + p->seg_size;
		    return q;
		}
		else
		    return p;
	    }
	}
    }
    else
    {
	/* Allocate memory from the beginning of free memory */
	for (p = segtab; p; p = p->seg_next)
	{
	    if (p->seg_count == 0) /* segment is not in use */
	    {
		if (curcontsize == 0)
		    curcontp = p;
		/* This is a follow-up on a free segment. */
		curcontsize += p->seg_size;
	    }
	    else
		curcontsize = 0; /* segment is in use */

	    /* remember the largest contiguous piece */
	    if (curcontsize > contsize)
	    {
		contsize = curcontsize;
		contp = curcontp;
	    }
	    if (p->seg_type == ST_UNUSED && p->seg_size >= size)
	    {
		/*
		** It fits. See whether we leave some space at the end.
		*/
		if (p->seg_size > size)
		{
		    q = segtab_alloc(p);
		    if (q == 0)
			return NILSEG;
		    q->seg_size = p->seg_size - size;
		    q->seg_begin = p->seg_begin + size;
		    p->seg_size = size;
		}
		return p;
	    }
	}
    }
    /*
    ** Nothing found. Check the contiguous space.
    */
    if (size >= contsize)
	return NILSEG;
    p = contp;
    while (p->seg_size < size)
    {
	/*
	** Merge with the next one.
	*/
	assert(p->seg_next);
	assert(p->seg_next->seg_count == 0);
	p->seg_size += p->seg_next->seg_size;
	segtab_free(p->seg_next);
    }
    p->seg_type = ST_UNUSED;
    if (p->seg_size > size)
    {
	if (direction == 1)
	{
	    q = segtab_alloc(p);
	    if (q == 0)
		return NILSEG;
	    q->seg_size = size;
	    p->seg_size -= size;
	    q->seg_begin = p->seg_begin + p->seg_size;
	    return q;
	}
	else
	{
	    if ((q = segtab_alloc(p)) ==  0)
		return NILSEG;
	    q->seg_size = p->seg_size - size;
	    q->seg_begin = p->seg_begin + size;
	    p->seg_size = size;
	}
    }
    assert(p->seg_size == size);
    return p;
}

/*
** mem_compact - concatenate all adjacent free segments
*/
static
mem_compact()
{
    register struct segtab *p;

    /*
     * This loop depends on merge_segment leaving p pointing to a
     * valid segtab entry, including updated seg_next field.
     */
    for (p = segtab; p != NILSEG; p = p->seg_next)
    {
	if (p->seg_count == 0)
	{
	    p->seg_type = ST_UNUSED;
	    merge_segment(p);
	}
    }
    /*MON_EVENT("compacting core");*/
}

/*-------------------------------------------------------------------
**
** Part 3: low-level segment handling. These (internal) routines
** allocate, free, grow and shrink memory segments.
**
**-------------------------------------------------------------------
*/
/*
** seg_alloc - allocate a segment.
*/
static struct segtab *
seg_alloc(size, type)
vir_clicks	size;
uint16		type;
{
    register struct segtab *p;
    int	direction;

    if (seg_init_level != SIL_FULL_INIT)
	panic("seg_init_done called at wrong moment\n");

    if (size <= 0)
	return 0;
    direction = (type == ST_USER || type == ST_GETBUF);
    if ((p = mem_findhole(size, direction)) == NILSEG)
    {
	mem_compact();
	if ((p = mem_findhole(size, direction)) == NILSEG)
	    return 0;
    }
    p->seg_type = type;
    p->seg_count = 1;
    p->seg_back = 0;
    return p;
}

/*
** seg_free - de-allocate a segment.
** NOTE: mem_compact relies on the fact that the argument is *never*
** freed using segtab_free(), i.e. that it is still a valid segment
** after the call to seg_free.
*/
void
seg_free(sp)
register struct segtab *sp;
{
    assert(sp != NILSEG);
    /*
     * It is possible that somebody already did an std_destroy on this
     * segment and the seg_count is 0.  It is being freed again because
     * the scavenger doesn't know it is gone.  We could stop if
     * seg_count == 0 but it doesn't hurt to do the rest of the routine
     * again and it will almost never happen.
     */
    assert(sp->seg_count >= 0);

    if (--sp->seg_count > 0)
	return;
#ifndef NOPROC
    /*
    ** Remove it from the mapping structure, if any.
    */
    if (sp->seg_back)
    	map_segfreed(sp->seg_back, sp);
#endif
    assert(!(sp->seg_type & (ST_KERNEL | ST_HARDWARE)));
    if (sp->seg_type & ST_ORIG)
	return;
    sp->seg_type = ST_UNUSED;
    merge_segment(sp);
}

/*
 * INVARIANT: upon return, sp in caller points to valid segtab entry
 *		(so prev entry or next entry has to be freed, not sp)
 */
static void
merge_segment(sp)
register struct segtab *sp;
{
    assert(sp);
    assert(sp->seg_count == 0);

    /*
    ** Try to merge with hole before us.
    */
    if (sp->seg_prev != NILSEG && sp->seg_prev->seg_type == ST_UNUSED)
    {
	sp->seg_begin = sp->seg_prev->seg_begin;
	sp->seg_size += sp->seg_prev->seg_size;
	segtab_free(sp->seg_prev);
    }
    /*
    ** Also try to n=merge with next hole.
    */
    if (sp->seg_next != NILSEG && sp->seg_next->seg_type == ST_UNUSED)
    {
	sp->seg_size += sp->seg_next->seg_size;
	segtab_free(sp->seg_next);
    }
}

#ifndef NOPROC
/*
** seg_grow - Grow/shrink a segment.
*/
struct segtab *
seg_grow(sp, down, size)
struct segtab *	sp;
int		down;
long		size;
{
    struct segtab *	nsp;	/* new segment pointer */
    int try;

    if (size == 0)
    {
	return NILSEG;
    }
    if (sp->seg_type & ST_HARDWARE)
    {
	DPRINTF(0, ("Growseg: attempt to grow hardware seg\n"));
	return NILSEG;
    }
    sp->seg_type &= ~(ST_CAPVALID | ST_ORIG);
    if (size < 0)
    {
	/*
	** We should shrink.
	*/
	if (sp->seg_size + size <= 0)
	{
	    DPRINTF(0, ("seg_grow: attempt to shrink to zero sized segment"));
	    return NILSEG;
	}
	if ((nsp = segtab_alloc(sp))== 0)
	    return NILSEG;
	sp->seg_type &= ~(ST_ORIG|ST_CAPVALID);
	if (down)
	{
	    /*
	    ** Should remove from beginning.
	    ** NOTE: further fiddling with refcounts etc is in map_grow
	    */
	    nsp->seg_type = sp->seg_type;
	    nsp->seg_begin = sp->seg_begin - size;
	    nsp->seg_size = sp->seg_size + size;
	    sp->seg_type = ST_UNUSED;
	    sp->seg_size = -size;
	    sp = nsp;
	}
	else
	{
	    nsp->seg_type = ST_UNUSED;
	    nsp->seg_begin = sp->seg_begin + sp->seg_size + size;
	    nsp->seg_size = -size;
	    sp->seg_size += size;
	}
	MON_EVENT("shrinking segment");
    }
    else
    {
	for (try=0; try<=1; try++) {
	    if (down)
		nsp = sp->seg_prev;
	    else
		nsp = sp->seg_next;
	    /*
	    ** See whether there is room there.
	    */
	    if (nsp && nsp->seg_count == 0 && nsp->seg_size >= size)
	    {
		nsp->seg_type &= ~(ST_ORIG|ST_CAPVALID);
		if (down)
		{
		    /*
		    ** Move the end of the unused segment into our seg.
		    */
		    nsp->seg_size -= size;
		    sp->seg_begin -= size;
		    sp->seg_size  += size;
		    phys_zero(CTOB(sp->seg_begin), CTOB(size));
		}
		else
		{
		    /*
		    ** Steal the beginning of the next segment.
		    */
		    phys_zero(CTOB(nsp->seg_begin), CTOB(size));
		    nsp->seg_size -= size;
		    nsp->seg_begin += size;
		    sp->seg_size  += size;
		}
		/*
		** See whether there is anything left in the hole.
		*/
		if (nsp->seg_size == 0)
		    segtab_free(nsp);
		MON_EVENT("growseg stealing next segment");
		break;
	    }
	    /*
	    ** Unfortunately moving segments is impossible.
	    ** There's lots of references to physical addresses:
	    ** await() args, trans/getreq/putrep header pointers, etc.
	    **
	    ** However, we have one last chance: we merge all holes,
	    ** and there might just be enough contiguous free
	    ** segments before/after the current one to satisfy the
	    ** request.
	    */
	    MON_EVENT("growseg failed");
	    if (try==0) {
		MON_EVENT("growseg: trying compaction");
		mem_compact();
	    } else {
		sp = 0;
	    }
	}
    }
    return sp;
}

/*
** seg_writeable - Make sure a segment is writeable.
** If the segment is shared a copy is returned, else the original
** segment.
*/
struct segtab *
seg_writeable(sp)
struct segtab *	sp;
{
    void	seg_free();

    struct segtab *	nsp;

    assert(sp);
    if (sp->seg_count > 1)
    {
	/*
	** Segment is shared, we should copy.
	*/
	nsp = seg_alloc(sp->seg_size, ST_USER);
	if (nsp == 0)
	    return 0;
	MON_EVENT("copy segment for write");
	phys_copy(CTOB(sp->seg_begin), CTOB(nsp->seg_begin), CTOB(sp->seg_size));
	seg_free(sp);
	return nsp;
    }
    sp->seg_type &= ~(ST_CAPVALID | ST_ORIG);
    return sp;
}
#endif /* NOPROC */


/*
** the following is intended for the bullet server but may find other
** uses in life.
**
**  seg_maxfree looks for the largest possible segment that it can allocate
**  in the system and returns its size.  This number can then be used in a
**  call to seg_alloc.
*/

vir_bytes
seg_maxfree()
{
    struct segtab *	p;
    long		max_size;    /* best contiguous free space size */
    long		cur_size;    /* current contiguous free space size */

    max_size = 0;
    cur_size = 0;

    /* for each segment we look to see which free segments are adjacent and
    ** form the largest free space
    */
    for (p = segtab; p; p = p->seg_next)
    {
	if (p->seg_count == 0) /* segment is not in use */
	    if (cur_size == 0)
		/* This is a free segment, and the previous one wasn't. */
		cur_size = p->seg_size;
	    else
		/* This is a follow-up on a free segment. */
		cur_size += p->seg_size;
	else
	    /* segment is in use - end of contiguous segment */
	    cur_size = 0;

	/* remember the largest contiguous piece */
	if (cur_size > max_size)
	    max_size = cur_size;
    }

/* now max_size points to the largest free space */
    return CTOB(max_size);
}


/*-------------------------------------------------------------------
**
** Part 4: kernel memory allocators. These come in three flavors:
** aalloc	Allocates non-returnable memory on an arbitrary boundary
** malloc/free	Normal malloc/free (see malloc.c in this directory)
** getblk/relblk These allocate click-aligned memory, in click-sized
**		amounts (a click is usually a MMU page, 512 bytes or so)
**
**------------------------------------------------------------------
*/
/*
** aalloc - allocate aligned memory.
**	    alignment must be on power of two
*/
char *
aalloc(size, align)
vir_bytes	size;
int		align;
{
    register vir_bytes	p;
    uint16		aldif;
    vir_clicks		needed;
    int			alignmask;
    
    assert(size > 0);
    assert(align >= 0);
    /*
    ** Zero align means align to something reasonable:
    */
    if (align == 0)
	align = sizeof(long);
    alignmask = align-1;
    assert((align & alignmask) == 0);
    assert(segtab != NILSEG);
    /*
    ** See whether we still have that much left in the current segment,
    */
    if (aalen < size+align)
    {
	needed = BSTOC(size+align);
	/*
	** First, try a dirty trick. If the next segment is empty
	** and big enough, steal some memory from it.
	*/
	if (aaseg && aaseg->seg_next &&
		aaseg->seg_next->seg_type == ST_UNUSED &&
		aaseg->seg_next->seg_size >= needed)
	{
	    register struct segtab *n = aaseg->seg_next;

	    n->seg_begin += needed;
	    n->seg_size  -= needed;
	    if (n->seg_size == 0)
		segtab_free(n);
	    aaseg->seg_size += needed;
	    aalen += CTOB(needed);
	}
	else
	{
	    /*
	    ** Allocate a new segment (and make sure it's big enough)
	    */
	    if ((aaseg = seg_alloc(needed, ST_AALLOC)) == 0) {
		panic("aalloc: out of memory");
	    }
	    aaptr = CTOB(aaseg->seg_begin);
	    aalen = CTOB(aaseg->seg_size);
	}
    }
    /*
    ** Fiddle pointers, clear the memory, and return.
    */
    for (p = aaptr; (long) p & alignmask; p++)
	;
    aldif = p - aaptr;
    aalen -= (aldif + size);
    aaptr += (aldif + size);
    phys_zero(p, size);
    return (char *) PTOV(p);
}

/*
** getblk - get memory that can be returned again.
*/
vir_bytes
getblk(size)
vir_bytes	size;
{
    register struct segtab *p;

    assert(segtab != NILSEG);
    p = seg_alloc(BSTOC(size), ST_GETBUF);
    if (p == 0)
    {
	DPRINTF(0, ("getblk: out of memory\n"));
	return (vir_bytes) p;
    }
    phys_zero(CTOB(p->seg_begin), CTOB(p->seg_size));
#if MEM_DEBUG
    printf("getblk(%d)= 0x%x\n", size, PTOV(CTOB(p->seg_begin)));
    stacktrace();
#endif
    return PTOV(CTOB(p->seg_begin));
}

/*
** relblk - return memory gotten from getblk.
*/
void
relblk(paddr)
vir_bytes	paddr;
{
    void	seg_free();

    register struct segtab *p;
    register vir_clicks addr = BATOC(VTOP(paddr));

    for (p = segtab; p; p = p->seg_next)
	if (p->seg_begin <= addr && p->seg_begin + p->seg_size > addr)
	{
	    assert(p->seg_type == ST_GETBUF);
	    assert(p->seg_count == 1);
	    seg_free(p);
	    return;
	}
    panic("relblk of non-getblk memory");
}

/*
** mgetblk - get memory used by malloc
*/
vir_bytes
mgetblk(size)
vir_bytes	size;
{
    register struct segtab *p;

    assert(segtab != NILSEG);
    p = seg_alloc(BSTOC(size), ST_MALLOC);
    if (p == 0)
    {
	DPRINTF(0, ("mgetblk: out of memory\n"));
	return (vir_bytes) p;
    }
    return PTOV(CTOB(p->seg_begin));
}
#ifndef NOPROC
/*-------------------------------------------------------------------
**
** Part 5: Capability getting/setting routines.
**
** The capability is in theory changed 'every access'.  Actually,
** all routines that map a segment in, write it, or whatever
** clear ST_CAPVALID.  This invalidates the capability and causes a
** new one to be created when needed.
**
**-------------------------------------------------------------------
*/

/*
** seg_setprv - Set segment capability.
*/
void
seg_setprv(sp, prv)
struct segtab *	sp;
private *	prv;
{
    if (! (sp->seg_type & (ST_USER | ST_HARDWARE))) {
	printf("seg_setprv: (fatal) sp=%x, sp->seg_type=%x\n",
							sp, sp->seg_type);
        assert(0);
    }
    if (!(sp->seg_type & ST_CAPVALID))
	uniqport(&sp->seg_random);
    (void) prv_encode(prv, (objnum) MKPRVNUM(OBJ_S, sp->seg_nr), PSR_ALL, 
	&sp->seg_random);
    sp->seg_type |= ST_CAPVALID;
}

/*
** seg_getprv - Get a segment given a capability.
*/
struct segtab *
seg_getprv(rights, prv)
rights_bits	rights;
private *	prv;
{
    objnum	segnum;		/* object number plus object type */
    int		num;		/* actual segment number */
    rights_bits	rrights;	/* required rights */
    struct segtab *sp;

    segnum = prv_number(prv);
    if (PRVTYPE(segnum) != OBJ_S)
	return 0;
    num = PRVNUM(segnum);
    if (num < 0 || num >= nseg)
	return 0;
    sp = segtab_rev[num];
    assert(sp->seg_nr == num);
    if (!(sp->seg_type & ST_CAPVALID))
    {
	DPRINTF(0, ("seg_getprv: old capability\n"));
	return 0;
    }
    assert(sp->seg_type & (ST_USER | ST_HARDWARE));
    if (prv_decode(prv, &rrights, &sp->seg_random) != 0 ||
					    (rights & rrights) != rights)
	return 0;
    return sp;
}

/*
** seg_restrict - Remove some rights from a capability.
*/
seg_restrict(priv, rights)
private *	priv;
rights_bits	rights;
{
    struct segtab *sp;

    if ((sp = seg_getprv(rights, priv)) == 0)
	return STD_CAPBAD;
    (void) prv_encode(priv, (objnum) MKPRVNUM(OBJ_S, sp->seg_nr), rights, 
							&sp->seg_random);
    return STD_OK;
}

/*
** seg_search - Search for the segment specified by {cap, off, len}.
*/
struct segtab *
seg_search(cap, off, len)
capability *	cap;
long		off;
long		len;
{
    struct segtab *sp;

    /*
    ** Is it one of our 'own' segments?
    */
    if (PORTCMP(&cap->cap_port, &segsvr_put_port))
    {
	sp = seg_getprv(PSR_READ | PSR_WRITE, &cap->cap_priv);
	if (sp == 0)
	    return sp;			/* Old segment */
	if (off == 0 && BSTOC(len) <= sp->seg_size)
	    return sp;
	else
	    return 0;
    }
    for (sp = segtab; sp; sp = sp->seg_next) {
	if ((sp->seg_type & ST_ORIG) && CAPCMP(cap, &sp->seg_orig) &&
			off == sp->seg_origoff && BSTOC(len) <= sp->seg_origlen)
	{
	    MON_EVENT("found cached segment");
	    return sp;
	}
    }
    return 0;
}


/*-----------------------------------------------------------------
**
** Part 6: miscellaneous interfaces. These routines are called by various
** other parts of the kernel, either to handle segments shared between
** a user program and a device (bitmap displays), or to obtain information
** (amount of free memory).
**
**-----------------------------------------------------------------
*/
/*
** HardwareSeg - Create a segment at a fixed address.
*/
HardwareSeg(cap, addr, len, shared)
capability *	cap;
phys_bytes	addr;
phys_bytes	len;
int		shared;
{
    struct segtab *	sp;
    phys_clicks		beg;

    if ((addr & (CLICKSIZE-1)))
	return STD_COMBAD;

    beg = BATOC(addr);

    /* See if we already have this segment lying around. */
    for (sp = segtab;
	     sp->seg_begin + sp->seg_size <= beg && sp->seg_next != NILSEG;
	     sp = sp->seg_next);
    {
	/* do nothing */;
    }
    if (!(sp->seg_type & ST_HARDWARE) || beg != sp->seg_begin)
    {
	/* We don't have it yet - so add it */
	sp = add_memchunk(beg, BSTOC(len),
		  (long) (shared ? (ST_HARDWARE | ST_HWSHARED) : ST_HARDWARE),
		  (long) 1);
	if (sp == NILSEG)
	    return STD_NOMEM;
    }
    else
    {
	/*
	 * We already have a hardware segment here.
	 * Check that it is the same as the "new" segment.
	 */
	if (sp->seg_begin != beg || sp->seg_size != BSTOC(len))
	    return STD_ARGBAD;
    }
    seg_setprv(sp, &cap->cap_priv);
    cap->cap_port = segsvr_put_port;
    return STD_OK;
}

#ifdef USER_DMA_SEGMENT
/*
** UserDmaSeg - Allow an existing kernel segment to be used for DMA
**		from user level.  Dangerous, but required for ultra
**		fast networking without kernel interference.
*/
errstat
UserDmaSeg(cap, addr, len)
capability *	cap;
phys_bytes	addr;
phys_bytes	len;
{
    struct segtab *	sp;
    phys_clicks		beg;

    beg = BATOC(addr);

    /* Find the segment corresponding to the address */
    for (sp = segtab;
	     sp->seg_begin + sp->seg_size <= beg && sp->seg_next != NILSEG;
	     sp = sp->seg_next);
    {
	/* do nothing */
    }
    if (beg < sp->seg_begin || beg >= sp->seg_begin + sp->seg_size) {
	/* No segment starting with the given address */
	return STD_NOTFOUND;
    }
    if (sp->seg_type & (ST_HARDWARE | ST_USER)) {
	/* Only allowed for segments allocated by the kernel itself */
	return STD_DENIED;
    }

    /* Maybe we should introduce a special ST_USERDMA bit? */
    sp->seg_type |= ST_HARDWARE;

    /* And generate a capability for it so that the segment can be
     * addressed from user level.
     */
    seg_setprv(sp, &cap->cap_priv);
    cap->cap_port = segsvr_put_port;
    return STD_OK;
}
#endif /* USER_DMA_SEGMENT */

/*
** SegAddr - Return physical address of a segment.
*/
phys_bytes
SegAddr(priv)
private *	priv;
{
    register struct segtab *sp;

    if ((sp = seg_getprv(PSR_READ | PSR_WRITE, priv)) == 0)
	return 0;
    sp->seg_count++;
    return CTOB(sp->seg_begin);
}

/*
** mem_free - Return amount of free memory.
*/
mem_free()
{
    register struct segtab *p;
    register long size;

    size = 0;
    for (p = segtab; p; p = p->seg_next)
	if (p->seg_count == 0)
	    size += p->seg_size;
    return size;
}

/*-------------------------------------------------------------------
**
** Part 7: the user interface routines. the seg_ routines are called
** indirectly (through mapsegment, for instance) and the usr_ routines
** are called directly from transactions.
** HardwareSeg and SegAddr are used by device drivers that share a
** segment with a user program, like bitmap displays.
**
**------------------------------------------------------------------
*/
/*
** seg_create - Create a segment, optionally fill it.
*/
errstat
seg_create(size, orig, spp, off, delete)
long			size;		/* Number of bytes */
capability *		orig;		/* Optional initial contents */
struct segtab **	spp;		/* Return value */
long			off;		/* Offset of initial contents in file */
long			delete;		/* True if we can delete it */
{
    register struct segtab *	sp;
    long			filled;		/* # bytes read into seg */
    errstat			rv;
    int				mustfill;

    /*XXXX Should this be an error? */
    if (delete && off != 0)
	printf("seg_create: warning: deleting partially mapped seg\n");
    if (size == 0)
    {
	printf("seg_create: attempt to create empty segment\n");
	return STD_ARGBAD;
    }
    if (orig && !NULLPORT(&orig->cap_port))
    {
	/*
	** See whether we happen to have a copy cached.
	*/
	sp = seg_search(orig, off, size);
	/*
	** Now see whether we can share it.
	*/
	if (sp)
	{
	    /*
	    ** We can. Increment refcount and return it.
	    */
	    sp->seg_count++;
	    *spp = sp;
	    if (delete) /* map and destroy */
	    {
		if (sp->seg_type & ST_ORIG) {
		    /*
		    ** We found a cached copy of the segment.
		    ** Go destroy the original.
		    */
		    (void) std_destroy(orig);
		} else {
		    /*
		    ** We found a segment that is owned by us.
		    ** Decrement the refcount.
		    */
		    sp->seg_count--;
		    assert(sp->seg_count > 0);
		    sp->seg_type &= ~ST_CAPVALID;
		    /*
		    ** If it belonged to another process (unmapped), steal it.
		    */
		    if (sp->seg_back)
			map_segfreed(sp->seg_back, sp);
		}
	    }
	    return STD_OK;
	}
	mustfill = 1;
    }
    else
    {
	mustfill = 0;
	filled = 0;
    }
    /*
    ** Otherwise, we allocate the segment and fill it.
    */
    if ((sp = seg_alloc(BSTOC(size), ST_USER)) == 0)
	return STD_NOSPACE;
    if (mustfill)
    {
	rv = b_read(orig, off, (char *) PTOV(CTOB(sp->seg_begin)),
				    (b_fsize) size, (b_fsize *) &filled);
	if (rv != STD_OK)
	{
	    printf("seg_create: could not read original: %s\n", err_why(rv));
	    printf("port is %p\n", &orig->cap_port);
	    seg_free(sp);
	    if (rv > 0)
		rv = STD_SYSERR;
	    return rv;
	}
	if (delete)
	    (void) std_destroy(orig);
	sp->seg_type |= ST_ORIG;
	sp->seg_orig = *orig;
	sp->seg_origoff = off;
	sp->seg_origlen = filled;
    }
    if (filled < size)
	phys_zero(CTOB(sp->seg_begin) + filled, (phys_bytes) (size - filled));
    *spp = sp;
    return STD_OK;
}

/*
** usr_segcreate - User routine to create segment.
*/
errstat
usr_segcreate(hdr, orig_seg)
header *	hdr;
capability *	orig_seg;	/* capability for segment to be cloned */
{
    errstat		rv;
    struct segtab *	sp;

    if ((rv = seg_create(hdr->h_offset, orig_seg, &sp, 0L, 0L)) == STD_OK)
    {
    /* put segment capability into header to be returned to user */
	seg_setprv(sp, &hdr->h_priv);
	hdr->h_port = segsvr_put_port;
    }
    return rv;
}

/*
** usr_segdelete - Delete a segment.
*/
errstat
usr_segdelete(priv)
private *	priv;
{
    void	seg_free();

    struct segtab *sp;

    if ((sp = seg_getprv(PSR_DELETE, priv)) == 0)
	return STD_CAPBAD;
    seg_free(sp);
    return STD_OK;
}

/*
** usr_segread - Read data from a segment.
*/
char *
usr_segread(hdr, rlen)
header *	hdr;
uint16 *	rlen;
{
    register struct segtab *sp;
    long	off = hdr->h_offset;
    long	len = hdr->h_size;

    *rlen = 0;
    if ((sp = seg_getprv(PSR_READ, &hdr->h_priv)) == 0)
    {
	hdr->h_status = STD_CAPBAD;
	return 0;
    }
    /*
    ** Cannot read hardware segments on some machines.
    */
    if (sp->seg_type & ST_HARDWARE)
    {
	hdr->h_status = STD_ARGBAD;
	return 0;
    }
    /*
    ** Check offset.
    */
    if (off < 0 || off >= CTOB(sp->seg_size))
    {
	if (off == CTOB(sp->seg_size))
	    hdr->h_status = STD_OK;
	else
	    hdr->h_status = STD_COMBAD;
	return 0;
    }
    if (off+len >= CTOB(sp->seg_size))
	len = CTOB(sp->seg_size) - off;
    hdr->h_size = *rlen = (uint16) len;
    hdr->h_status = STD_OK;
    MON_EVENT("read segment request");
    return (char *) PTOV(CTOB(sp->seg_begin) + off);
}


/*
** usr_segsize - return size of data segment
*/
errstat
usr_segsize(hdr)
header *	hdr;
{
    register struct segtab *sp;

    if ((sp = seg_getprv(PSR_READ, &hdr->h_priv)) == 0)
	return STD_CAPBAD;
    hdr->h_offset = CTOB(sp->seg_size);
    return STD_OK;
}


/*
** usr_segwrite - Write data to a segment.
*/
errstat
usr_segwrite(priv, off, buf, len, rlen)
private	*	priv;
long		off;
char *		buf;
long		len;
uint16 *	rlen;
{
    register struct segtab *sp;

    if ((sp = seg_getprv(PSR_WRITE, priv)) == 0)
	return STD_CAPBAD;

    /*
    ** Cannot read hardware segments on some machines.
    */
    if (sp->seg_type & ST_HARDWARE)
	return STD_ARGBAD;

    /*
    ** Check offset.
    */
    if (off < 0 || off >= CTOB(sp->seg_size))
    {
	*rlen = 0;
	return STD_ARGBAD;
    }

    if (off + len > CTOB(sp->seg_size))
	len = CTOB(sp->seg_size) - off;

    phys_copy(VTOP(buf), CTOB(sp->seg_begin) + off, (phys_bytes) len);
    *rlen = (uint16) len;
    MON_EVENT("write segment request");
    return STD_OK;
}

#endif /* NOPROC */
/*-------------------------------------------------------------------
**
** Part 8: miscelaneous stuff. The initialize and dump routines.
**
**------------------------------------------------------------------
*/
#ifndef SMALL_KERNEL
/*
** segdump - dump segments in a reasonable way.
*/
int
segdump(begin, end)
char *	begin;	/* start of buffer to hold dump */
char *	end;	/* end of buffer */
{
    char *		p;
    struct segtab *	stp;

    p = bprintf(begin, end, "Num     begin     size count type\n");
    for (stp = segtab; stp; stp = stp->seg_next)
    {
	p = bprintf(p, end, "%3d  %8lx %8lx %5d", stp->seg_nr,
		CTOB(stp->seg_begin), CTOB(stp->seg_size), stp->seg_count);
	if (stp->seg_type == ST_UNUSED)
	    p = bprintf(p, end, " FREE\n");
	else
	{
	    if (stp->seg_type & ST_USER)     p = bprintf(p, end, " USER");
	    if (stp->seg_type & ST_KERNEL)   p = bprintf(p, end, " KERNEL");
	    if (stp->seg_type & ST_AALLOC)   p = bprintf(p, end, " AALLOC");
	    if (stp->seg_type & ST_GETBUF)   p = bprintf(p, end, " GETBLK");
	    if (stp->seg_type & ST_HARDWARE) p = bprintf(p, end, " HARDWARE");
	    if (stp->seg_type & ST_ORIG)     p = bprintf(p, end, " ORIGINAL");
	    if (stp->seg_type & ST_CAPVALID) p = bprintf(p, end, " CAPVALID");
	    if (stp->seg_type & ST_NOMEM)    p = bprintf(p, end, " NOMEM");
	    if (stp->seg_type & ST_MALLOC)   p = bprintf(p, end, " MALLOC");
	    if (stp->seg_type & ST_HWSHARED) p = bprintf(p, end, " HWSHARED");
	    p = bprintf(p, end, "\n");
	}
    }
    if (aaseg)
	p = bprintf(p, end, "Aalloc segno = %d, ptr = %lx, len = %lx\n",
				    aaseg->seg_nr, (long) aaptr, (long) aalen);
    p = bprintf(p, end, "nseg = %d\n", nseg);
    return p - begin;
}
#endif /* SMALL_KERNEL */

/*
** Initmem - init seg table.
*/


static struct segtab small_segtable[N_SEGS];

void
seg_init()
{
    int i;
    struct segtab *p;

    if (seg_init_level != SIL_NO_INIT)
	panic("seg_init called at wrong moment\n");

    /* Build free segment table */
    p = small_segtable;
    freesegtab = p;
    for (i = 0; i < N_SEGS; p++, i++)
    {
	p->seg_nr = i;
	p->seg_next = p + 1;
    }
    small_segtable[N_SEGS - 1].seg_next = NILSEG;
    segtab = NILSEG;

    seg_init_level = SIL_FIRST_INIT;
}


/*
 * Register a chunk of physical memory as an unused segment.
 */
void
seg_reg_memchunk(start, size)
phys_clicks start;
phys_clicks size;
{
    if (seg_init_level != SIL_FIRST_INIT)
	panic("seg_reg_memchunk called at wrong moment\n");

    nseg += size; /* nseg starts life as the # clicks - we work on it later */
    if (add_memchunk(start, size, (long) ST_UNUSED, (long) 0) == NILSEG)
	panic("seg_reg_memchunk: couldn't add chunk to segtab");
}


/*
 * Mark the specified chunk of memory as in use.  It must be on the list
 * of free memory!!
 */
void
seg_mark_used(start, size, type)
phys_clicks start, size;
long type;
{
    struct segtab *sp;
    struct segtab *newseg;

    if (seg_init_level != SIL_FIRST_INIT)
	panic("seg_mark_used called at wrong moment\n");

    if (size == 0)  /* Ignore things smaller than a click */
	return;

    nseg -= size - 1; /* This memory is used and only takes 1 segment */

    /* Find where in the list it belongs */
    sp = segtab;
    while (sp->seg_begin + sp->seg_size < start && sp->seg_next != NILSEG)
	sp = sp->seg_next;

    if (sp->seg_begin > start ||
		sp->seg_begin + sp->seg_size < start + size)
	panic("seg_mark_used: marking chunk that doesn't fit");

    /* Don't bother splitting things of the same type */
    if (sp->seg_type == type)
	return;

    if (sp->seg_type != ST_UNUSED)
	panic("seg_mark_used: overlapped memory usage");

    if (sp->seg_begin == start)
    {
	if (sp->seg_size != size)
	{
	    /*
	     * Take from front of segment - we actually move this segment
	     * to be newseg and install the new one in sp.
	     */
	    newseg = segtab_alloc(sp);
	    assert(newseg);
	    newseg->seg_begin = sp->seg_begin + size;
	    newseg->seg_size = sp->seg_size - size;
	    newseg->seg_count = sp->seg_count;
	    newseg->seg_type = sp->seg_type;
	    sp->seg_begin = start;
	    sp->seg_size = size;
	}
	sp->seg_count = 1;
	sp->seg_type = type;
    }
    else
    {
	/* Take from middle or back of segment */
	newseg = segtab_alloc(sp);
	assert(newseg);
	newseg->seg_begin = start;
	newseg->seg_size = size;
	newseg->seg_type = type;
	newseg->seg_count = 1;
	sp->seg_size -= size;
	if (start != sp->seg_begin + sp->seg_size)
	{
	    /* There is still a chunk at the back to fill in */
	    newseg = segtab_alloc(newseg);
	    assert(newseg);
	    newseg->seg_count = sp->seg_count;
	    newseg->seg_type = sp->seg_type;
	    newseg->seg_begin = start + size;
	    newseg->seg_size = sp->seg_begin + sp->seg_size - start;
	    sp->seg_size -= newseg->seg_size;
	}
    }
}


#define	CLK16K_SHIFT	14	/* log(16k) */

/*
 * Now that all the memory chunks are registered we can build the real
 * segment table.  We leave that until now because in the 386 if the
 * kernel just fits in 640K the nseg table must be allocated in hi-mem
 * which is otherwise rather tricky.
 */
void
seg_init_done()
{
    struct segtab *seg;
    struct segtab *f;
    int i;

    if (seg_init_level != SIL_FIRST_INIT)
	panic("seg_init_done called at wrong moment\n");
    seg_init_level = SIL_FULL_INIT;

    /*
     * Now to calculate nseg.  At present it is # clicks in system.
     * If the click size is large we will need more segments than if the
     * clicks are small!  Furthermore, kernels have a nasty tendency to
     * make lots of little segments for group code, etc, so we had better
     * not let things get too small.
     */
    nseg -= N_SEGS;
#if CLICKSHIFT < CLK16K_SHIFT /* clicksize < 16K */
    {
	int	tmpnseg;

	/* Don't accept less than 100 segments */
	if ((tmpnseg = nseg >> (CLK16K_SHIFT - CLICKSHIFT)) > 100)
	    nseg = tmpnseg;
    }
#endif

    segtab_rev = (struct segtab **) calloc((size_t) (nseg + N_SEGS),
					  sizeof(struct segtab *));
    assert(segtab_rev);
    for (i = 0; i < N_SEGS; i++)
	segtab_rev[i] = &small_segtable[i];
	
    seg = (struct segtab *) calloc((size_t) nseg, sizeof(struct segtab));
    assert(seg);
    (void) memset((_VOIDSTAR) seg, '\0', (size_t) nseg * sizeof(struct segtab));

    /* Attach new segtab structs to end of free list */
    for (f = freesegtab; f->seg_next != NILSEG; f = f->seg_next)
	/* do nothing */;
    f->seg_next = seg;

    /* Build free list pointers in new segtab structs */
    for (i = 0; i < nseg; seg++, i++)
    {
	segtab_rev[i + N_SEGS] = seg;
	seg->seg_nr = i + N_SEGS;
	seg->seg_next = seg + 1;
    }
    seg[-1].seg_next = NILSEG;
    nseg += N_SEGS;
}
