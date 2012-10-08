/*	@(#)findhole.c	1.5	96/02/27 11:22:00 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * This code is used principly by malloc but it has to cope with
 * other callers too.
 *
 * The principle behind this version is that the only entity that can
 * modify the segment map is the process itself.  Therefore, instead of
 * continually asking getinfo() for the segment info we do it just once and
 * thereafter track the state of the segments via the seg_ calls.
 * The segment info is an array of segments by segment id with a linked list
 * running through it of the allocated segments sorted by decreasing start
 * address * (sd_addr).
 *
 * Author:  Gregory J. Sharp, June 1994
 */

#include "amoeba.h"
#include "stdlib.h"
#include "machdep.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"
#include "module/mutex.h"

#define	MAX_NSEG	((MAX_PDSIZE / 2) / sizeof (segment_d))
#define	NILCAP		((capability *) 0)
#define	CLICK_ALIGN(a)	(((a) + CLICKSIZE - 1) & ~(CLICKSIZE - 1))

extern segid	_sysseg_map _ARGS(( capability *, vir_bytes, vir_bytes, long ));
extern errstat	_sysseg_unmap _ARGS(( segid, capability * ));
extern errstat	_sysseg_grow _ARGS(( segid, vir_bytes ));

typedef struct seg_d	seg_d;
struct seg_d {
    seg_d *		sd_next;	/* a list of segments ordered */
    seg_d *		sd_prev;	/* by start address */
    unsigned long	sd_addr;	/* start address of segment */
    long		sd_len;		/* real size of segment */
};

/* Our segment data needs some memory in bss to work from */
static seg_d	Seg_tab[ MAX_NSEG ];

/* Ordered linked list of segments in use */
static seg_d *	Active_list;

/* To protect all these data structures */
static mutex	mu;

/* And to spare us from excessive initialisation */
static int	Initialised;


/* List management routines */
/****************************/

/*
 * Try to insert the new segment descriptor into the active list.
 * If it overlaps an existing entry then we abort since the kernel
 * should have detected that.
 */

static void
lst_insert_in_active(sp)
seg_d *	sp;
{
    seg_d *	cp;	/* Current position in active list */
    seg_d *	np;	/* Position immediately after cp */

    if (sp->sd_len <= 0)
	abort();

    cp = Active_list;
    if (cp == 0)
    {
	Active_list = sp; /* The first item on the list */
	return;
    }
    /* Does it go on the front of the list? */
    if (sp->sd_addr > cp->sd_addr)
    {
	/* Insert sp at the beginning of the list */
	if (cp->sd_addr + cp->sd_len > sp->sd_addr)
	{
	    /* They overlap!! */
	    abort();
	}
	sp->sd_next = cp;
	cp->sd_prev = sp;
	Active_list = sp;
	return;
    }
    
    /* Otherwise we search for the right place in the list to insert it */
    for (np = cp->sd_next; np != 0 && np->sd_addr > sp->sd_addr; )
    {
	cp = np;
	np = np->sd_next;
    }

    /* Insert it between cp and np */
    if (np != 0)
    {
        if (sp->sd_addr + sp->sd_len > cp->sd_addr || /* sp overlaps cp? */
	    np->sd_addr + np->sd_len > sp->sd_addr)   /* np overlaps sp? */
	{
	    /* They overlap!! */
	    abort();
	}
	np->sd_prev = sp;
    }
    sp->sd_next = np;
    sp->sd_prev = cp;
    cp->sd_next = sp;
}


static void
lst_delete_from_active(sp)
seg_d *	sp;
{
    if (sp->sd_len == 0)
	abort();	/* Deleting a deleted entry! */

    if (sp->sd_next != 0)
	sp->sd_next->sd_prev = sp->sd_prev;
    if (sp->sd_prev != 0)
	sp->sd_prev->sd_next = sp->sd_next;
    else
	Active_list = sp->sd_next;
    sp->sd_next = 0;
    sp->sd_prev = 0;
    sp->sd_len = 0;
}


/*
 * We need a small buffer to hold the process descriptor of the initial
 * process.  This value of BSZ allows us to hold up to 14 segment descriptors.
 * Almost every process will begin life with only 4 segments.
 */
#define	BSZ	512

static void
lst_init()
{
    char	buf[BSZ];
    process_d *	pd;
    segment_d *	sd;
    seg_d *	sp;

    /* This routine assumes that a mu_lock(&mu) is in effect! */
    if (!Initialised)
    {
	Initialised = 1;
	pd = (process_d *) buf;
	if (getinfo(NILCAP, pd, BSZ) <= 0)
	    abort();
	for (sp = Seg_tab, sd = PD_SD(pd);
				 sd < PD_SD(pd) + pd->pd_nseg; sp++, sd++)
	{
	    if (sd->sd_type != 0)
	    {
		sp->sd_next = 0;
		sp->sd_prev = 0;
		sp->sd_addr = sd->sd_addr;
		sp->sd_len = CLICK_ALIGN(sd->sd_len);
		lst_insert_in_active(sp);
	    }
	}
    }
}


/* The wrappers for the seg_map family of system calls. */
/********************************************************/

/* These routines keep the user version of the segment descriptors up to date */
segid
seg_map(cap, addr, len, flags)
capability *	cap;
vir_bytes	addr;
vir_bytes	len;
long		flags;
{
    segid	id;
    seg_d *	sp;

    mu_lock(&mu);
    if (!Initialised)
	lst_init();
    id = _sysseg_map(cap, addr, len, flags);
    if (id >= 0)
    {
	sp = &Seg_tab[id];
	if (sp->sd_len != 0) /* segment id was already in use */
	    abort();
	sp->sd_addr = addr;
	sp->sd_len = CLICK_ALIGN(len);
	lst_insert_in_active(sp);
    }
    mu_unlock(&mu);
    return id;
}


errstat
seg_unmap(id, cap)
segid		id;
capability *	cap;
{
    errstat	err;

    mu_lock(&mu);
    if (!Initialised)
	lst_init();
    err = _sysseg_unmap(id, cap);
    if (err == STD_OK)
	lst_delete_from_active(&Seg_tab[id]);
    mu_unlock(&mu);
    return err;
}


errstat
seg_grow(id, newsize)
segid		id;
vir_bytes	newsize;
{
    errstat	err;

    mu_lock(&mu);
    if (!Initialised)
	lst_init();
    err = _sysseg_grow(id, newsize);
    if (err == STD_OK)
    {
	Seg_tab[id].sd_len = CLICK_ALIGN(newsize);
    }
    mu_unlock(&mu);
    return err;
}


/* findhole - Look for a hole in VM space */
/******************************************/

/*
 * Algorithm: look through all the segments in the current process for a
 * hole in the mapped virtual addresses (the hole above the stack segment
 * is not legal).  If a hole can be found that starts GAP bytes after the
 * end of a mapped segment and has len bytes, and then at least GAP bytes
 * before the start of the next segment then we return the address of the
 * hole, else 0.
 * An additional requirement is imposed by Unix compatibility: the hole
 * should start above 'end' (ie. BSS).
 */

#ifdef __SUNPRO_C
#define end _end
#endif
extern char end; /* End of BSS segment (as calculated by the linker) */

#define PROPOSED_GAP (32*1024)

#if PROPOSED_GAP >= CLICKSIZE
#define GAP PROPOSED_GAP
#else
#define GAP CLICKSIZE
#endif

char *
findhole(len)
long	len;
{
    seg_d *		sp;	/* current segment */
    unsigned long	addr;	/* address of end of segment sp */

    if (len <= 0)
	return 0; /* Don't ask for zero or negative size holes */

    /*
     * We want to reserve GAP bytes on either side of the hole,
     * so increase len by 2*GAP.
     */
    if ((len += 2 * GAP) <= 0)
	return 0; /* Overflow occurred; len is too big anyway */

    mu_lock(&mu);

    if (!Initialised)
	lst_init();

    if ((sp = Active_list) == 0)
    {
	mu_unlock(&mu);
	return 0;
    }

    /*
     * For each segment except the highest, which we step over, see if the
     * hole of length 'len' immediately after it doesn't overlap other
     * segments.
     * Hacker's Note:
     *   I could make this code faster but it would be less clear.
     *   If efficiency turns out to be a problem I'll do it.
     */
    for (sp = sp->sd_next;
	 sp != 0 && (char *) (addr = sp->sd_addr + sp->sd_len) >= &end;
	 sp = sp->sd_next)
    {
	unsigned long	v_end;

	if (sp->sd_len == 0) /* Look for inconsistencies */
	    abort();

	/* Calculate where our new segment would end if it started at addr */
	v_end = addr + len;

	/* if (no overflow occurred && it fits) */
	if (v_end > addr && v_end <= sp->sd_prev->sd_addr)
	{
	    mu_unlock(&mu);
	    return (char *) (addr + GAP);
	}
    }

    mu_unlock(&mu);
    return 0;
}
