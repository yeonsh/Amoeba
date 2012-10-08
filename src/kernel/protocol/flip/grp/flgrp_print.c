/*	@(#)flgrp_print.c	1.9	96/02/27 14:02:23 */
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
#include "flgrp_member.h"
#include "flgrp_alloc.h"
#include "flgrp_hstpro.h"
#include "module/ar.h"
#include "sys/proto.h"

/* Debugging stuff and dump stuff. */

extern group_p grouptab;
extern uint16 grp_maxgrp;

#ifndef SMALL_KERNEL

#ifdef UNUSED
int grp_dumpblock(begin, end, g)
    char *begin;
    char *end;
    group_p g;
{
    barrier_p bp;
    char *p = begin;

    for(bp = g->g_fbarrier; bp != 0; bp = bp->b_next) {
	p = bprintf(p, end, "0x%X ", bp);
    }
    p = bprintf(p, end, "f: 0x%X l: 0x%X\n", g->g_fbarrier, g->g_lbarrier);
    return(p - begin);
}
#endif


int grp_dumpflag(begin, end, f)
    char *begin;
    char *end;
    uint32 f;
{
    char *p;
    
    p = bprintf(begin, end, "flags: ");
    if(f & FL_LOCATING) p = bprintf(p, end, "locating ");
    if(f & FL_RECEIVING) p = bprintf(p, end, "receiving ");
    if(f & FL_SENDING) p = bprintf(p, end, "sending ");
    if(f & FL_JOINING) p = bprintf(p, end, "joining ");
    if(f & FL_RESET) p = bprintf(p, end, "reset ");
    if(f & FL_VOTE) p = bprintf(p, end, "vote ");
    if(f & FL_RESULT) p = bprintf(p, end, "result ");
    if(f & FL_COHORT) p = bprintf(p, end, "cohort ");
    if(f & FL_COORDINATOR) p = bprintf(p, end, "coordinator ");
    if(f & FL_SYNC) p = bprintf(p, end, "hist-full ");
    if(f & FL_DESTROY) p = bprintf(p, end, "dying ");
    if(f & FL_NOUSER) p = bprintf(p, end, "no-user ");
    if(f & FL_FAILED) p = bprintf(p, end, "failed ");
    if(f & FL_QUEUE) p = bprintf(p, end, "queue ");
    if(f & FL_SIGNAL) p = bprintf(p, end, "signaled ");
    if(f & FL_COLLECTING) p = bprintf(p, end, "collecting ");
    p = bprintf(p, end, "\n");
    return(p - begin);
}


int grp_dump(begin, end)
char *begin;
char *end;
{
    char *p;
    group_p g;
    
    p = bprintf(begin, end, "========= group status dump ==============\n");
    for(g = grouptab; g < grouptab + grp_maxgrp; g++) {
	if(!g->g_used) continue;
	p = bprintf(p, end, "===== group: %d %s: =====\n", g - grouptab,
		    ar_port(&g->g_port));
	p += grp_dumpflag(p, end, g->g_flags);
	p = bprintf(p , end, "large: %d debug: %d resilience: %d ", 
		    g->g_large, grp_debug_level, g->g_resilience);
	p = bprintf(p, end, "incarnation: %u\n", g->g_incarnation);
	p = bprintf(p, end, "member: %u ", g->g_index);
	p = bprintf(p, end, "sequencer: %u ", g->g_seqid);
	p = bprintf(p, end, "coordinator: %d ",
		    (g->g_coord != 0) ? (g->g_coord - g->g_member) : -1);
	p = bprintf(p, end, "sender: %d ",
	      (g->g_last_sender != 0) ? (g->g_last_sender - g->g_member) : -1);
	p = bprintf(p, end, "rank: %u ", g->g_memrank);
	p = bprintf(p, end, "# member: %u\n", g->g_total);
	p = bprintf(p, end, "seqno:%U minhist %d nexthistory %U nhstfull %U\n", 
		    g->g_nextseqno, g->g_minhistory, g->g_nexthistory,
		    g->g_nhstfull);
	p = bprintf(p, end, "maxgroup: %u ", g->g_maxgroup);
	p = bprintf(p, end, "nbuf: %u ", g->g_nbuf);
	p = bprintf(p, end, "nhist: %u ", g->g_nhist);
	p = bprintf(p, end, "maxsizemess: %U\n", g->g_maxsizemess);
	p = bprintf(p, end, 
		    "rtimer %d stimer %d atimer %d rtime %d stime %d atime %d\n", 
		    g->g_replytimer, g->g_synctimer, g->g_alivetimer,
		    g->g_rtimeout, g->g_stimeout, g->g_atimeout);
	p = bprintf(p, end, 
		    "resettimer %d resettimeout %d maxretrial %d\n",
		    g->g_resettimer, g->g_resettimeout, g->g_maxretrial);
	p = bprintf(p, end, "buf: free %d hist %d mem %d\n",
		    g->g_bufnbuf, g->g_hstnbuf, g->g_memnbuf);
#ifndef NO_CONGESTION_CONTROL
	p = bprintf(p, end, "roundtrip: min %ld max %ld avg %ld sdv %ld retrans %ld\n",
		    g->g_cng_min_roundtrip, g->g_cng_max_roundtrip,
		    g->g_cng_avg_roundtrip, g->g_cng_sdv_roundtrip,
		    g->g_cng_retrans);
	p = bprintf(p, end, "rate: cur %ld threshold %ld sent %ld total %ld\n",
		    g->g_cng_cur_rate, g->g_cng_thresh_rate,
		    g->g_cng_sent, g->g_cng_tot_sent);
#endif
	p = bprintf(p, end, "group addresses: ");
	p += badr_print(p, end, &g->g_addr);
	p = bprintf(p, end, " ");
	p += badr_print(p, end, &g->g_addr_all);
	p = bprintf(p, end, "\n");
	p += grmem_dump(p, end, g);
    }
    p = bprintf(p, end, "==========================================\n");
    return(p - begin);
}
#endif
