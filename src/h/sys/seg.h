/*	@(#)seg.h	1.6	96/02/27 10:39:23 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __SEG_H__
#define __SEG_H__

struct segtab {
    phys_clicks	seg_begin;	/* where does this segment start */
    vir_clicks	seg_size;	/* how many clicks does it occupy */
    struct segtab  *seg_prev;	/* segment before this one */
    struct segtab  *seg_next;	/* segment after this one */
    long	seg_type;	/* Type of segment. */
    long	seg_count;	/* usage count */
    capability	seg_orig;	/* Original */
    long	seg_origoff;	/* Offset in original */
    long	seg_origlen;	/* # bytes of original */
    port	seg_random;	/* Random port */
    struct mapping *seg_back;	/* Back pointer to process */
    int		seg_nr;		/* segment number */
};

/* Types of segments, for seg_type: */
#define ST_UNUSED	0
#define ST_USER		0x01		/* CreateSegment type seg */
#define ST_KERNEL	0x02		/* Kernel text/data segment */
#define ST_AALLOC	0x04		/* aalloc type seg */
#define ST_GETBUF	0x08		/* Getbuf type segment */
#define ST_HARDWARE	0x10		/* Hardware (i.e. fixed) segment */
#define ST_ORIG		0x20		/* Still has original contents */
#define ST_CAPVALID	0x40		/* Random port is valid */
#define ST_NOMEM	0x80		/* No memory at this location */
#define	ST_MALLOC	0x100		/* Kernel malloc memory */
#define ST_HWSHARED	0x200		/* Shared access OK (e.g. timer dev) */

#define NILSEG ((struct segtab *)0)

/*
** Macros to convert between clicks and bytes.
** CTOB - clicks to bytes.
** BATOC - byte address to click number.
** BSTOC - byte size to number of clicks.
*/
#define CTOB(x) ((vir_bytes)(x)<<CLICKSHIFT)
#define BATOC(x) ((vir_clicks)((vir_bytes)(x) >> CLICKSHIFT))
#define BSTOC(x) BATOC((x)+CLICKSIZE-1)

void seg_mark_used _ARGS(( phys_clicks start, phys_clicks size, long type ));
void seg_reg_memchunk _ARGS(( phys_clicks start, phys_clicks size ));

#endif /* __SEG_H__ */
