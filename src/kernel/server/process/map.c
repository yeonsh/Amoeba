/*	@(#)map.c	1.10	96/02/27 14:20:39 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** map - handle memory mapping, machine independent part.
**
** Author: Jack Jansen, CWI, 22-Sep-86.
** Modified beyond recognition by everyone else since then.
*/
/*
** NOTE: this should disappear. All 'struct process' parameters should
** be changed to 'struct mapping **'.
*/
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "machdep.h"
#include "global.h"
#include "exception.h"
#include "kthread.h"
#include "seg.h"
#include "process/proc.h"
#include "procdefs.h"
#include "map.h"
#include "sys/proto.h"
#include "monitor.h"
#include "sys/flip/measure.h"

#define NILMAP		((struct mapping *) 0)

extern struct process *	curusercl;
extern port		segsvr_put_port;

extern void		seg_free();
extern errstat		seg_create();

/************************************/
/* mapping struct allocation module */
/************************************/

static struct mapping *	mapfree; /* Free list of map structures */

/* We allocate 100 mapping structs each time we need to allocate new ones */
#define	NMAPS		100
#define	NMAPS_BYTES	(NMAPS * sizeof (struct mapping))

#define	FREE_MAP(m) \
	do {				\
	    (m)->mp_next = mapfree;	\
	    mapfree = (m);		\
	} while (0)

/*
** alloc_map -  allocate a "struct mapping".  If we run short then we allocate
**		some extra memory to hold some more.  When we first started
**		up we allocated a reasonably large chunk of the things so
**		we shouldn't have to allocate extra very often.
*/
static struct mapping *
alloc_map()
{
    struct mapping * mp;

    if ((mp = mapfree) == NILMAP)
    {
	DPRINTF(0, ("alloc_map: allocating %d mapping structs\n", NMAPS));
	mapfree = (struct mapping *) aalloc((vir_bytes) NMAPS_BYTES, 0);

	/* Make the linked list */
	for (mp = mapfree; mp < mapfree + NMAPS - 1; mp++)
	    mp->mp_next = mp + 1;
	mapfree[NMAPS - 1].mp_next = NILMAP;
	mp = mapfree;
    }
    assert(mp != NILMAP);
    mapfree = mapfree->mp_next;
    mp->mp_next = 0;
    return mp;
}

/************************************/

/*
** umap - convert user-virtual address to kernel-physical.
**
** Note: the use of this routine assumes that all virtual address space
** of all user programs is always accessible to the kernel.
**
** As a side effect of this the cache can be flushed.
*/

#ifdef NO_FAST_UMAP
phys_bytes
umap(t, vir, len, might_modify)
struct thread *	t;
vir_bytes	vir;
vir_bytes	len;
int might_modify;
{
    register vir_clicks xvir, xlen;
    register struct mapping *mp;
    phys_bytes result;

    assert(t != NILTHREAD);
    if ((mp = GetThreadMap(t)) == (struct mapping *) 0)
	return (phys_bytes) VTOP(vir);	/* Kernel process. */

    DPRINTF(2, ("umap(%d, %x, %x, %d)\n", THREADSLOT(t), vir, len, might_modify));
    xvir = BATOC(vir);
    xlen = BATOC(vir + len - 1) - xvir;
    while (mp)
    {
	if (mp->mp_type != 0 && 
		xvir >= mp->mp_addr && xvir < mp->mp_addr + mp->mp_len)
	{
	    if (xlen > mp->mp_len || xvir + xlen >= mp->mp_addr + mp->mp_len)
	    {
		DPRINTF(0, ("umap: thread %d: spans segment boundary(%x, %x)=(%x, %x) not in (%x, %x)\n",
		THREADSLOT(t), vir,len, xvir, xlen, mp->mp_addr, mp->mp_len));
		putsig(t, (signum) EXC_SYS);
		return 0;
	    }
	    if (mp->mp_seg->seg_type & ST_HARDWARE) {
		DPRINTF(0, ("umap: thread %d: attempt to use hardware segment\n",
							    THREADSLOT(t)));
		putsig(t, (signum)EXC_SYS);
		return 0;
	    }
	    if (might_modify ) {
		if ((mp->mp_type & MAP_PROTMASK) != MAP_READWRITE) {
		    DPRINTF(0, ("umap: thread %d: attempt to write readonly segment\n",
							    THREADSLOT(t)));
		    putsig(t, (signum)EXC_SYS);
		    return 0;
		}
	    }
	    result = CTOB(mp->mp_seg->seg_begin) + (vir - CTOB(mp->mp_addr));
	    return result;
	}
	mp = mp->mp_next;
    }
    DPRINTF(0, ("umap: thread %d: illegal address %x\n", THREADSLOT(t), vir));
    putsig(t, (signum) EXC_SYS);
    return 0;
}

#else

/* Just using the truncated virclicks as the hash value should work fine: */
#define UMAP_HASH(xvir)  ((xvir) & (NUM_UMAP_HASH - 1))

phys_bytes
umap(t, vir, len, might_modify)
struct thread *	t;
vir_bytes	vir;
vir_bytes	len;
int		might_modify;
{
    /* An attempt to make umap() faster.
     * The idea is to keep a hashed index to the segment map, based on the
     * clicks in the virtual adress.
     * The hashed index is invalidated whenever the mapping changes.
     */
    register vir_clicks xvir, xlen;
    register struct mapping *mp;
    struct process *cl;
    phys_bytes result;
    register int uhash;

    /* BEGIN_MEASURE(map_ptr); */
    assert(t != NILTHREAD);

    xvir = BATOC(vir);
    xlen = BATOC(vir + len - 1) - xvir;
    uhash = UMAP_HASH(xvir);
    DPRINTF(2, ("umap(%d, %x, %x, %d): hash %d: ",
		THREADSLOT(t), vir, len, might_modify, uhash));
    /* temp HACK; need to call function/macro from mpx.c */
#   undef tk_mpx
    cl = t->tk_mpx.MX_process;
    assert(cl != NULL);
    mp = cl->pr_map_index[uhash];
    if ((mp != NULL) && /* (mp->mp_type != 0) && */
	(xvir >= mp->mp_addr) && (xvir + xlen < mp->mp_addr + mp->mp_len))
    {
	DPRINTF(2, ("hit\n"));
	/* The cached pointer was only installed after all other
	 * checks passed, so they don't have done again.
	 * E.g., we know that the segment is valid and writable.
	 */
	return CTOB(mp->mp_seg->seg_begin) + (vir - CTOB(mp->mp_addr));
    }
    else
    {
	DPRINTF(2, ("miss\n"));
    }

    /* have to do linear search */
    mp = cl->pr_map;
    if (mp == NULL) {
	return (phys_bytes) VTOP(vir);	/* Kernel process. */
    }
    
    while (mp)
    {
	if (mp->mp_type != 0 && 
		xvir >= mp->mp_addr && xvir < mp->mp_addr + mp->mp_len)
	{
found:
	    if (xlen > mp->mp_len || xvir + xlen >= mp->mp_addr + mp->mp_len)
	    {
		DPRINTF(0, ("umap: thread %d: spans segment boundary(%x, %x)=(%x, %x) not in (%x, %x)\n",
		THREADSLOT(t), vir,len, xvir, xlen, mp->mp_addr, mp->mp_len));
		putsig(t, (signum) EXC_SYS);
		return 0;
	    }
	    if (mp->mp_seg->seg_type & ST_HARDWARE) {
		DPRINTF(0, ("umap: thread %d: attempt to use hardware segment\n",
							    THREADSLOT(t)));
		putsig(t, (signum)EXC_SYS);
		return 0;
	    }
	    if (might_modify) {
		if ((mp->mp_type & MAP_PROTMASK) != MAP_READWRITE) {
		    DPRINTF(0, ("umap: thread %d: attempt to write readonly segment\n",
							    THREADSLOT(t)));
		    putsig(t, (signum)EXC_SYS);
		    return 0;
		}
	    }
	    if ((mp->mp_type & MAP_PROTMASK) == MAP_READWRITE) {
		/* update hash index for writable segment */
		cl->pr_map_index[uhash] = mp;
	    }
	    result = CTOB(mp->mp_seg->seg_begin) + (vir - CTOB(mp->mp_addr));
	    /* END_MEASURE(map_ptr); */
	    return result;
	}
	mp = mp->mp_next;
    }
    DPRINTF(0, ("umap: thread %d: illegal address %x\n", THREADSLOT(t), vir));
    putsig(t, (signum) EXC_SYS);
    return 0;
}

static void
map_invalidate_index(cl)
struct process *cl;
{
    /* Invalidate the hash index on the mapping structure after when
     * it changes.  For safety, also invalidate the mapped version
     * of the local ptr.
     */
    int i;

    for (i = 0; i < NUM_UMAP_HASH; i++) {
	cl->pr_map_index[i] = NULL;
    }
    cl->pr_map_local_ptr = 0;
}

#endif

/*
** map_vafree - return true if the VA range is free of other segments.
*/
map_vafree(cl, xaddr, xlen)
    struct process *cl;
    vir_clicks xaddr, xlen;
{
    struct mapping *mp;

    for (mp = cl->pr_map; mp; mp = mp->mp_next)
    {
	if (mp->mp_type == 0)
	    continue;
	if (!( mp->mp_addr+mp->mp_len<=xaddr || xaddr+xlen<=mp->mp_addr))
	    return 0;
    }
    return 1;
}

/*
** map_segaddr - Internal version of mapseg.
**
**  For each process there is a linked list of mapping structures.  You can
**  only add and delete mapping structures at the tail of the list.  If a
**  map is freed in the middle of the list it is marked unused and can be
**  reused later.  The reason for this is that the map_segment system call
**  tells the user process a per-process segment id and for some bizarre
**  reason this is not in the mapping struct but is simply its position in
**  the linked list of mapping structs.
**
**	INVARIANT:  sum of reference counts on all segments
*/
static segid
map_segaddr(cl, sp, xaddr, xlen, type)
struct process *	cl;
struct segtab  **	sp;
vir_clicks		xaddr;
vir_clicks		xlen;
long			type;
{
    register struct mapping *mp;
    register struct mapping **mpp;
    segid rv;
    struct segtab *nsp;


    nsp = *sp;
    assert(cl);
    assert(nsp);
    if (nsp->seg_size != xlen)
    {
	printf("map: warning: %d clicks asked, segsize %d\n",
							xlen, nsp->seg_size);
	assert(nsp->seg_size >= xlen);
    }
    /*
    ** Sanity check on arguments.
    */
    if ((type & MAP_PROTMASK) == 0 || (type & MAP_TYPEMASK) == 0)
    {
	printf("map_segaddr: funny type %x\n", type);
	return STD_COMBAD;
    }
    /*
    ** Check that the specified virtual address range is free.
    */
    if (!map_vafree(cl, xaddr, xlen))
	return STD_ARGBAD;
    /*
    ** Now make sure that we can map the segment for writing, if wanted.
    */
    if ((type & MAP_PROTMASK) == MAP_READWRITE && !(type & MAP_INPLACE))
    {
	if ((nsp = seg_writeable(nsp)) == 0)
	{
	    /*
	    ** Probably means no space.
	    */
	    printf("map_segaddr: segwriteable failed\n");
	    return STD_NOSPACE;
	}
	*sp = nsp;
    }

    if (type & MAP_INPLACE) {
	if (nsp->seg_count == 1) {
	    printf("map_segaddr: non-local segment\n");
	    return STD_ARGBAD;
	} else {
	    /* Shared access is sensible for some devices (e.g., user timer)
	     * but not for others (e.g., video card, network adapter).
	     */
	    if (((nsp->seg_type & ST_HWSHARED) == 0) && (nsp->seg_count > 2)) {
		DPRINTF(0, ("map_segaddr: tried to map unsharable device\n"));
		return STD_ARGBAD;
	    }
	}
    }
    /*
    ** Now, insert new mapping.
    ** First, try to find a mapping descriptor in the linked list that is free,
    ** i.e. unmapped (mp_type==0) and not associated with an existing
    ** segment (mp_seg==0). Otherwise, allocate a new mapping structure.
    */
    for (rv=0, mp=cl->pr_map ; mp ; rv++, mp=mp->mp_next) {
	if (mp->mp_type == 0 && mp->mp_seg == 0) {
	    break;
	}
    }
    if (mp == 0) {
	rv = -1;
	if ((mp = alloc_map()) == NILMAP)
	    return STD_NOSPACE;
    }
    mp->mp_clus = cl;
    mp->mp_addr = xaddr;
    mp->mp_len  = xlen;
    mp->mp_seg  = nsp;
    mp->mp_type = type;
    DPRINTF(2, ("a=%x,l=%x,s=%x,t=%x\n", mp->mp_addr, mp->mp_len, mp->mp_seg, mp->mp_type));
#ifdef CACHE_DMA_SEG
    /* setmap() currently makes user level DMA segments uncached by default.
     * For some user level device drivers that know what they're doing
     * this is not really necessary.  Therefore we enable it conditionally
     * by not passing the MAP_INPLACE flag to setmap in this case.
     */
    if (((nsp->seg_type & ST_HARDWARE) != 0) &&
        ((nsp->seg_type & ST_HWSHARED) == 0) &&
        ((nsp->seg_type & ST_AALLOC) != 0))
    {
	type &= ~MAP_INPLACE;
    }
#endif
    if (setmap((uint16) PROCSLOT(cl), xaddr, nsp->seg_begin, xlen, type) < 0)
    {
	if (rv < 0) {
	    /* Re-used old mapping, zap it */
	    mp->mp_seg = 0;
	    mp->mp_type = 0;
	} else {
	    /* Allocated new structure, free it */
	    FREE_MAP(mp);
	}
	return STD_ARGBAD;
    }
    if (rv < 0) {
	/* We allocated a new structure. Insert at the end */
	for (rv = 0, mpp = &cl->pr_map; *mpp; mpp = &((*mpp)->mp_next))
	    rv++;
	*mpp = mp;
    }
    MON_EVENT("map segment");
    return rv;
}

/*
** map_segment - Map segment into a process.
*/
segid
map_segment(cl, addr, len, type, orig, off)
struct process *	cl;
vir_bytes		addr;
vir_bytes		len;
long			type;
capability *		orig;
long			off;
{
    vir_clicks		addr_c;
    vir_clicks		len_c;
    errstat		rv;
    segid		sid;
    struct segtab *	sp;

    if (cl == 0)
	cl = curusercl;
    /*
    ** First, convert and check arguments.
    */
    addr_c = BATOC(addr);
    len_c  = BSTOC(len);
    DPRINTF(2, ("addr_c=%x, len_c=%x\n", addr_c, len_c));
    DPRINTF(2, ("addr=%x, len=%x\n", addr, len));
    if (CTOB(addr_c) != addr)
    {
	printf("map_segment: boundary wrong: %x (%x)\n", addr, CTOB(addr_c));
	return STD_ARGBAD;
    }
    if (addr_c < VIR_LOW || (addr_c+len_c) > VIR_HIGH)
    {
	printf("map_segment: segment addresses out of range: %x(%x)\n",
								addr_c, len_c);
	return STD_ARGBAD;
    }

    /*
    ** Now create segment.
    */
    rv = seg_create((long) CTOB(len_c), orig, &sp, off, type & MAP_AND_DESTROY);
    if (rv != STD_OK)
	return rv;
    /*
    ** And map it.
    */
    sid = map_segaddr(cl, &sp, addr_c, len_c, type & ~MAP_AND_DESTROY);
    if (sid < 0)
	seg_free(sp);
    return sid;
}

/*
** usr_mapseg - Map a segment given a descriptor.
**		We have to deal with a type blending here so it looks a little
**		weird.
*/
errstat
usr_mapseg(cl, md)
struct process *	cl;
segment_d *		md;
{
    if (map_segment(cl, (vir_bytes) md->sd_addr, (vir_bytes) md->sd_len,
			md->sd_type, &md->sd_cap, md->sd_offset) < 0)
	return STD_SYSERR;
    else
	return STD_OK;
}

/*
** mapfind - Find a given mapping in a process.
*/
struct mapping *
map_find(cl, mapid)
struct process *	cl;
segid			mapid;
{
    register struct mapping *mp;

    for (mp = cl->pr_map; mp && --mapid >= 0; mp = mp->mp_next)
	;
    return mp;
}

/*
** usr_unmap - Unmap system call from the user.
*/
errstat
usr_unmap(cl, mapid, newcap)
struct process *	cl;
segid			mapid;
capability *		newcap;
{
    register struct mapping *mp;

    if (cl == 0)
	cl = curusercl;
    mp = map_find(cl, mapid);
    if (!mp || mp->mp_type == 0)
    {
	printf("usr_unmap: no such mapid\n");
	return STD_COMBAD;
    }
#ifndef NO_FAST_UMAP
    map_invalidate_index(cl);
#endif
    /*
    ** Clear mmu tables.
    */
    (void) setmap((uint16) PROCSLOT(cl), mp->mp_addr, (phys_clicks) 0,
							    mp->mp_len, -1L);
    assert(mp->mp_seg);
    MON_EVENT("unmap segment");
    if (newcap)
    {
	seg_setprv(mp->mp_seg, &newcap->cap_priv);
	newcap->cap_port = segsvr_put_port;
    }
    /*
    ** Plant a pointer so seg_free() knows to call map_segfreed().
    */
    mp->mp_seg->seg_back = mp;
    
    mp->mp_type = 0;
    mp->mp_len = 0;
    mp->mp_addr = 0;
    /*
    ** if we aren't sending back a capability to the user we can get rid of
    ** the segment
    */
    if (newcap == 0)
	seg_free(mp->mp_seg);
    return STD_OK;
}


/*
** map_delmap - Delete all segments belonging to a process.
*/
void
map_delmap(cl)
struct process *	cl;
{
    register struct mapping *	mp;
    register struct mapping *	nmp;

#ifndef NO_FAST_UMAP
    map_invalidate_index(cl);
#endif
    for (mp = cl->pr_map; mp; mp = nmp)
    {
	nmp = mp->mp_next;
	if (mp->mp_seg)
	    seg_free(mp->mp_seg);
	FREE_MAP(mp);
    }
    clrmap((uint16) PROCSLOT(cl));
    MON_EVENT("unmap all segments");
}


/*
** GetNmap - Get number of segments.
*/
int
GetNmap(cl)
struct process *	cl;
{
    register struct mapping *mp;
    register int n = 0;

    if (cl == 0)
	cl = curusercl;
    for (mp = cl->pr_map; mp; mp = mp->mp_next)
	n++;
    return n;
}

/*
** seg_count - get number of segments belonging to a process.
*/
int
seg_count(cl)
struct process *	cl;
{
    struct mapping *	mp;
    int			nsegs;
    
    for (nsegs = 0, mp = cl->pr_map; mp; mp = mp->mp_next, nsegs++)
	;
    return nsegs;
}

/*
** getmap - reconstruct the mapping descriptors for a process.
**
** this routine is called from two places: from the getinfo system
** call (with running==1), in which case the segments are not touched,
** and from the owner-warning code (in case of an exception) with
** running == 0, in which case all segments are given a name.
*/
int
getmap(cl, sd, maxmap, running)
struct process *	cl;
segment_d *		sd;
int			maxmap;
int			running;
{
    struct mapping *	mp;
    uint16		nmap;

    nmap = 0;
    for (mp = cl->pr_map; mp; mp = mp->mp_next)
    {
	if ((int) nmap++ >= maxmap)
	    return 0;
	if (mp->mp_seg != 0 && (!running || mp->mp_type == 0))
	{
	    seg_setprv(mp->mp_seg, &sd->sd_cap.cap_priv);
	    sd->sd_cap.cap_port = segsvr_put_port;
	}
	else
	    (void) memset((_VOIDSTAR) &sd->sd_cap, 0, sizeof (capability));
	sd->sd_offset = 0;
	if (mp->mp_type != 0)
	{
	    sd->sd_addr = mp->mp_addr << CLICKSHIFT;
	    sd->sd_len  = mp->mp_len  << CLICKSHIFT;
	    sd->sd_type = mp->mp_type | MAP_AND_DESTROY;
	}
	else
	{
	    sd->sd_addr = 0;
	    sd->sd_len  = 0;
	    sd->sd_type = 0;
	}
	sd++;
    }
    return nmap;
}

/*
** map_grow - User system call to grow a segment.
*/
errstat
map_grow(mapid, size)
segid	mapid;
long	size;
{
    register struct mapping *mp;
    register struct segtab *sp, *osp;
    vir_clicks xaddr, xlen;

    /* get map entry */
    mp = map_find(curusercl, mapid);
    if (!mp || mp->mp_type == 0 || (mp->mp_type & MAP_GROWMASK) == MAP_GROWNOT)
    {
	DPRINTF(0, ("map_grow: no such mapid\n"));
	return STD_ARGBAD;
    }

    /* First see that the VA space is free, if we're actually growing */
    xlen = BSTOC(size) - mp->mp_len;
    if (xlen > 0) {
	if ((mp->mp_type&MAP_GROWMASK)==MAP_GROWDOWN) {
	    xaddr = mp->mp_addr - xlen;
	} else {
	    xaddr = mp->mp_addr + mp->mp_len;
	}
	if (!map_vafree(curusercl, xaddr, xlen)) {
	    DPRINTF(2, ("map_grow: not free\n"));
	    return STD_ARGBAD;
	}
    }
    /* Compute new virtual address */
    if ((mp->mp_type & MAP_GROWMASK) == MAP_GROWDOWN)
	xaddr = mp->mp_addr - xlen;
    else
	xaddr = mp->mp_addr;
    /* try to grow the segment */
    osp = mp->mp_seg;
    if ( osp->seg_count != 1 ) {
	DPRINTF(0, ("map_grow: shared segment\n"));
	return STD_ARGBAD;
    }
    sp = seg_grow(osp, (mp->mp_type & MAP_GROWMASK) == MAP_GROWDOWN,
	 (long) (BSTOC(size) - mp->mp_len));
    if (sp == 0)
    {
	MON_EVENT("map_grow: seg_grow failed");
	return STD_NOSPACE;
    }
    /* Fiddle the refcounts, if needed */
    if ( sp != osp ) {
	assert( osp->seg_back == 0);
	assert( osp->seg_count == 1);
	assert( sp->seg_count == 0);
	mp->mp_seg = sp;
	sp->seg_count = 1;
	osp->seg_count = 0;
    }
#ifndef NO_FAST_UMAP
    /* may not be necessary */
    map_invalidate_index(curusercl);
#endif
    /* Zap the old mapping */
    if (setmap((uint16) PROCSLOT(curusercl), mp->mp_addr, (phys_clicks) 0,
							mp->mp_len, -1L) < 0)
    {
	MON_EVENT("map_grow: clearing old map failed");
	printf("map_grow: clearing old map failed");
    }
    mp->mp_seg = sp;
    mp->mp_addr = xaddr;
    mp->mp_len = BSTOC(size);
    /* And install the new one */
    if (setmap((uint16) PROCSLOT(curusercl), mp->mp_addr, sp->seg_begin,
						mp->mp_len, mp->mp_type) < 0)
    {
	MON_EVENT("map_grow: setmap failed");
	return STD_NOSPACE;
    }
    MON_EVENT("grow segment");
    return STD_OK;
}

/*
** Called from seg_free() when destroying a segment that still belongs
** to a process (although unmapped); this function clears the segment
** pointer in the mapping structure so it won't be freed again at process
** exit.
*/
map_segfreed(mp, sp)
struct mapping *	mp;
struct segtab *		sp;
{
    assert(mp);
    assert(sp);
    assert(mp->mp_seg == sp);
    assert(sp->seg_back == mp);
    /* Etc. :-) */
    mp->mp_seg = 0;
    sp->seg_back = 0;
}
