/*	@(#)flgrp_hist.c	1.10	96/02/27 14:01:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h" 
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "assert.h"
INIT_ASSERT
#include "group.h"
#include "flgrp_header.h"
#include "flgrp_hist.h"
#include "flgrp_type.h"
#include "flgrp_alloc.h"
#include "flgrp_hstpro.h"
#include "byteorder.h"
#include "../rpc/am_endian.h"
    
/* This module implements the history buffer management of a group. */

#define LH	2

#define GRP_FREE_HIST(hist)			\
	if ((hist)->h_size > 0) {		\
	    g->g_hstnbuf--;			\
	    GRP_FREEBUF(g, (hist)->h_data);	\
	    (hist)->h_size = 0;			\
	    (hist)->h_bc.b_seqno = 0;		\
	}
    
int hst_free(g)
    group_p g;
{
/* Try to free buffers from the history. */

    register uint32 lb;
    register hist_p hist;
    register member_p mem;
    
    lb = g->g_me->m_expect;
    /* If resilience is nonzero, or we are the sequencer, we have to
     * see what messages the other members might still be needing later on.
     */
    if (g->g_resilience > 0 || g->g_index == g->g_seqid) {
	for (mem = g->g_member; mem < g->g_member + g->g_maxgroup; mem++) {
	    if (mem->m_state == M_MEMBER || mem->m_state == M_LEAVING) {
		GDEBUG(LH, printf("hst_free: member %d expects %d\n",
				  mem - g->g_member, mem->m_expect));
		lb = MIN(lb, mem->m_expect);
	    }
	}
    }
    assert(lb >= g->g_minhistory && lb <= g->g_nextseqno);
    GDEBUG(LH, printf("hst_free: purge history %d .. %d\n",
		      g->g_minhistory, lb));
    for (; g->g_minhistory < lb; g->g_minhistory++) {
	hist = &g->g_history[HST_MOD(g, g->g_minhistory)];
	GRP_FREE_HIST(hist);
    }
    return(!HST_SYNCHRONIZE(g));
}

int
hst_free_one_buf(g)
    group_p g;
{
    /* Try to free one buffer from the history. */
    hist_p hist, max_hist;
    int s, max_seq;
    uint16 freebufs;

    freebufs = g->g_bufnbuf;
    (void) hst_free(g);
    if (g->g_bufnbuf > freebufs) {
	GDEBUG(1, printf("hist_free_buf: freed old hist bufs\n"));
	return 1;
    }

    /* remove the latest unprocessed one */
    max_seq = 0;
    max_hist = 0;
    for (s = 0; s < (int) g->g_nhist; s++) {
	hist = &g->g_history[s];
        if (hist->h_bc.b_seqno >= g->g_nextseqno && hist->h_data != 0) {
	    if (hist->h_bc.b_seqno > max_seq) {
		max_hist = hist;
		max_seq = hist->h_bc.b_seqno;
	    }
	}
    }
    if (max_hist != 0) {
	GRP_FREE_HIST(max_hist);
	GDEBUG(1, printf("hist_free_buf: freed hist buf %d\n", max_seq));
	return 1;
    }

    /* TODO: another thing we could do is removal of buffered messages
     * from the other members (BB protocol).  See grpmem_buffree().
     */
    GDEBUG(1, { printf("hist_free_buf: no free buffers\n"); hst_print(g); });
    return 0;
}

hist_p hst_append(g, bc, data, size, bigendian)
    register group_p g;
    bchdr_p bc;
    char *data;
    f_size_t size;
    int bigendian;
{
/* Append message to history. */

    hist_p h;
    long lb;
    
    assert(size >= sizeof(header));
    assert(data != 0);
    assert(g->g_nexthistory - g->g_me->m_expect < g->g_nhist);
    assert(g->g_minhistory <= g->g_me->m_expect);
    assert(bc->b_seqno == g->g_nexthistory);
    if(g->g_index != g->g_seqid) {
	lb = MAX((long) bc->b_seqno - (long) g->g_nhist,
		 (long) (g->g_nexthistory + 1) - (long) g->g_nhstfull);
	if(lb > (long) g->g_minhistory && lb <= (long) g->g_me->m_expect) {
	    g->g_minhistory = lb;
	}
    }
    assert(g->g_minhistory <= g->g_me->m_expect);
    h = &g->g_history[HST_MOD(g, g->g_nexthistory)];
    GRP_FREE_HIST(h);
    h->h_bc = *bc;
    h->h_nack = 0;
    h->h_mem_mask = 0;
    h->h_accept = 0;

    g->g_hstnbuf++;
    h->h_data = data;
    h->h_size = size;
    h->h_bigendian = bigendian;
    am_orderhdr((header *)data, bigendian);

    g->g_nexthistory++;
    assert(HST_CHECK(g));
    return h;
}


int hst_store(g, bc, data, size, bigendian, accept)
    group_p g;
    bchdr_p bc;
    char *data;
    f_size_t size;
    int bigendian;
    int accept;
{
/* Message is out of order. Store it in the right place. */

    hist_p h;
    long lb;
    
    assert(size >= sizeof(header));
    assert(data != 0);
    assert(g->g_nexthistory - g->g_me->m_expect < g->g_nhist);
    assert(g->g_minhistory <= g->g_me->m_expect);
    if(g->g_index != g->g_seqid) {
	lb = MAX((long) bc->b_seqno - (long) g->g_nhist,
		 (long) g->g_nexthistory - (long) g->g_nhstfull);
	if (lb > (long) g->g_minhistory) {
	    if (lb < g->g_me->m_expect) {
		g->g_minhistory = lb;
	    } else {
		/* We cannot store this message since we're running too
		 * far behind the sequencer.  We'll have to ask for a
		 * retransmission later on.
		 */
		return 0;
	    }
	}
    }
    assert(g->g_minhistory <= g->g_me->m_expect);
    h = &g->g_history[HST_MOD(g, bc->b_seqno)];
    GRP_FREE_HIST(h);
    h->h_bc = *bc;
    h->h_nack = 0;
    h->h_mem_mask = 0;
    h->h_accept = accept;

    g->g_hstnbuf++;
    h->h_data = data;
    h->h_size = size;
    h->h_bigendian = bigendian;
    am_orderhdr((header *)data, bigendian);

    assert(HST_CHECK(g));
    return 1;
}


hist_p hst_lookup(g, cpu)
    group_p g;
    uint16 cpu;
{
/* Find last message that is sent for cpu. */

    uint32 i;
    member_p mem = &g->g_member[cpu];
    bchdr_p bc;
    
    /* Search backwards, because messages from two different members
     * with the same member-id may temporarily be present in the history.
     */
    for(i = g->g_nextseqno; i >= g->g_minhistory; i--) {
	bc = &g->g_history[HST_MOD(g, i)].h_bc;
	if(bc->b_cpu == cpu && bc->b_messid == mem->m_messid)
	    return(&g->g_history[HST_MOD(g, i)]);
    }
    return(0);
}


hist_p hst_buf_lookup(g, cpu)
    group_p g;
    uint16 cpu;
{
/* Find last message in second part of the history that is sent for cpu. */

    uint32 i;
    member_p mem = &g->g_member[cpu];
    bchdr_p bc;
    
    for(i= g->g_nextseqno; i < g->g_nexthistory; i++) {
	bc = &g->g_history[HST_MOD(g, i)].h_bc;
	if(bc->b_cpu == cpu && bc->b_messid == mem->m_messid)
	    return(&g->g_history[HST_MOD(g, i)]);
    }
    return(0);
}


void hst_buffree(g)
    group_p g;
{
/* Free history buffers that are not accepted yet and undo all updates to the
 * state info of the member whose message is buffered.
 */

    uint32 i;
    hist_p hist;
    uint32 s;

    /* First empty the second part of the history. Only the sequencer buffers
     * messages in this part.
     */
    for(i = g->g_nextseqno; i < g->g_nexthistory; i++) {
	assert(g->g_seqid == g->g_index);
	hist = &g->g_history[HST_MOD(g, i)];
	GRP_FREE_HIST(hist);
	GDEBUG(LH, printf("hst_buffree: purge history %d (cpu %d, id %d)\n",
			  g->g_minhistory, hist->h_bc.b_cpu,
			  g->g_member[hist->h_bc.b_cpu].m_messid));
	g->g_member[hist->h_bc.b_cpu].m_messid--;
    }
    g->g_nexthistory = g->g_nextseqno;		/* remove second part. */

    /* Empty third part of the history. Only members buffer messages in
     * this part.
     */
    for(s = 0; s < g->g_nhist; s++) {
	hist = &g->g_history[s];
        if(hist->h_bc.b_seqno >= g->g_nextseqno) { /* a buffered msg? */
	    assert(g->g_seqid != g->g_index);
	    GRP_FREE_HIST(hist);
	}
    }
}


void hst_print(g)
    group_p g;
{
    uint32 i;
    
    printf("=========== history ==============\n");
    printf("minhst %d seqno %d nexthist %d expect %d\n", g->g_minhistory,
	   g->g_nextseqno, g->g_nexthistory, g->g_me->m_expect);
    for(i= 0; i < g->g_nhist; i++) {
	printf("i : %d ", i);
	bc_print(&g->g_history[HST_MOD(g, i)].h_bc);
	printf("\n");
    }
    printf("==================================\n");
}
