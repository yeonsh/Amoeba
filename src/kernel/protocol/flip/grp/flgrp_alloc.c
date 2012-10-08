/*	@(#)flgrp_alloc.c	1.6	96/02/27 14:01:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h" 
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "group.h"
#include "flgrp_header.h"
#include "flgrp_hist.h"
#include "flgrp_type.h"
#include "flgrp_alloc.h"
#include "flgrp_hstpro.h"
#include "flgrp_malloc.h"

/*
 * This module implements the buffer management for a group.
 * It is really just a simple wrapper around a malloc() variant.
 */

static char *
grp_alloc_seg(memdesc, wanted)
Mem_descr *memdesc;
vir_bytes wanted;
{
    char *seg;

    /* TODO: maybe we should put some upper bound to the amount we
     * allocate in total for the group.  This could be dependent on
     * message, history and group size.  On the other hand, when
     * memory is plenty, and the application just happens to be sending
     * large messages, why worry?
     */
    seg = (char *) getblk(wanted);
    GDEBUG(1, printf("alloc new seg at 0x%lx (size 0x%lx)\n", seg, wanted));
    return seg;
}

static void
grp_free_seg(memdesc, seg)
Mem_descr *memdesc;
char *seg;
{
    GDEBUG(1, printf("freeing seg at 0x%lx\n", seg));
    relblk((vir_bytes) seg);
}

/*
 * Defines for the segment size heuristic in grp_initbuf():
 */
#define LARGE_MESSAGE	8192
#define LARGE_GROUP	8	/* actually, this isn't "large" at the VU */
#define LARGE_HISTSIZE	128
#define HEADER_OVERHEAD	128	/* rough guess at malloc header overhead */
#define LARGE_CHUNCK	((1024 * 1024) - HEADER_OVERHEAD)
#define SMALL_CHUNCK	((256 * 1024) - HEADER_OVERHEAD)

void grp_initbuf(g)
group_p g;
{
    unsigned int chunck_size;

    /* We allocate memory required by a group member in large chuncks
     * in order to avoid segment fragmentation.
     * We use an heuristic for the chunck size: 
     * - for heavy duty work (large messages and many members or
     *   large history size) use large segments of about 1 Meg.
     * - otherwise use segments of about 256K.
     */
    if (g->g_maxsizemess >= LARGE_MESSAGE &&
	(g->g_maxgroup >= LARGE_GROUP || g->g_nhist >= LARGE_HISTSIZE))
    {
	chunck_size = LARGE_CHUNCK;
    }
    else
    {
	chunck_size = SMALL_CHUNCK;
    }
    mal_init_descr(&g->g_mem_descr, grp_alloc_seg,
		   grp_free_seg, chunck_size);
}
