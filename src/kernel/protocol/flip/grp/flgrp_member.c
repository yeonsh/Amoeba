/*	@(#)flgrp_member.c	1.11	96/02/27 14:02:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h" 
#include "assert.h"
INIT_ASSERT
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "group.h"
#include "flgrp_header.h"
#include "flgrp_hist.h"
#include "flgrp_type.h"
#include "flgrp_member.h"
#include "flgrp_alloc.h"
#include "flgrp_hstpro.h"
#include "sys/proto.h"

/* This module implements the member management of a group. */

#define MON_EVENT(str)	printf("%s\n", str);
#define SETTIMER(g, msg) \
	do { \
		int settimer(); \
		(msg)->p_admin.pa_released = settimer; \
		(msg)->p_admin.pa_ident = (int) (g); \
	} while(0)

extern group_p grouptab;


#ifndef SMALL_KERNEL
int grmem_dump(begin, end, g)
    char *begin, *end;
    group_p g;
{
    char *p;
    int i;
    member_p m;
    
    if (g->g_member == 0) {
	/* no member status, currently */
	return(0);
    }

    p = begin;
    for(m = g->g_member, i = 0; m < g->g_member + g->g_maxgroup; i++, m++) {
	if(m->m_state == M_UNUSED) continue;
	p = bprintf(p, end, "%d (%d) : ", i, m->m_state);
	p += badr_print(p, end, &m->m_addr);
	p = bprintf(p, end, " rpc ");
	p += badr_print(p, end, &m->m_rpcaddr);
	p = bprintf(p, end,
	    " expect %U mid %U retrial %d roff %U total %U vote %d reply %d\n",
		    m->m_expect, m->m_messid,
		    m->m_retrial, m->m_roffset, m->m_total,
		    m->m_vote, m->m_replied);
    }
    return(p - begin);
}
#endif


int grmem_member(g, addr, m)
    group_p g;
    adr_p addr;
    g_index_t *m;
{
    int i;
    
    for(i=0; i < (int) g->g_maxgroup; i++) {
	if(g->g_member[i].m_state == M_UNUSED)  continue;
	if(ADR_EQUAL(&g->g_member[i].m_addr, addr)) {
	    *m = (g_index_t) i;
	    return 1;
	}
    }
    return 0;
}


int grmem_alloc(g, new)
    group_p g;
    g_index_t *new;
{
    g_index_t i;
    
    /* Note: this function used to reuse an entry of a leaving member,
     * when no free one was available.  Since that member could have
     * missed its own LEAVE message, this is considered too risky.
     * A joining member now may have to try a few more times before
     * it can take the place of the member that left.
     */
    for(i=0; i < g->g_maxgroup; i++) {
	if(g->g_member[i].m_state == M_UNUSED) {
	    g->g_member[i].m_data = 0;
	    *new = i;
	    return(1);
	}
    }
    printf("grmem_alloc: out of member entries\n");
    return(0);
}


void grmem_del(g, m)
    group_p g;
    member_p m;
{
    m->m_state = M_UNUSED;
    if(m->m_data) {
	g->g_memnbuf--;
	GRP_FREEBUF(g, m->m_data);
    }
}


void grmem_buffer(g, src, bc, data, n, bigendian)
    group_p g;
    member_p src;
    bchdr_p bc;
    char *data;
    f_size_t n;
{
    if(src->m_bufdata != 0) {
	g->g_memnbuf--;
	GRP_FREEBUF(g, src->m_bufdata);
    }
    g->g_memnbuf++;
    src->m_nack = 0;
    src->m_bufdata = data;
    src->m_bufsize = n;
    src->m_bufbc = *bc;
    src->m_bigendian = bigendian;
}


#ifdef UNUSED
void grmem_buffree(g)
    group_p g;
{
    member_p m;

    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(m->m_state != M_MEMBER) continue;
	if(m->m_data) {
	    g->g_memnbuf--;
	    GRP_FREEBUF(g, m->m_data);
	}
	if(m->m_bufdata) {
	    g->g_memnbuf--;
	    GRP_FREEBUF(g, m->m_bufdata);
	}
    }
}
#endif


#ifdef __STDC__
void grmem_new(group_p g, g_index_t i, adr_p sa, adr_p rpcaddr,
					g_msgcnt_t messid, g_seqcnt_t seqno)
#else
void grmem_new(g, i, sa, rpcaddr, messid, seqno)
    group_p g;
    g_index_t i;
    adr_p sa;
    adr_p rpcaddr;
    g_msgcnt_t messid;
    g_seqcnt_t seqno;
#endif
{
    member_p m;
    
    m = g->g_member + i;
    m->m_state = M_MEMBER;
    m->m_addr = *sa;
    m->m_rpcaddr = *rpcaddr;
    m->m_messid = messid;
    m->m_expect = seqno;
    m->m_retrial = g->g_maxretrial;
    m->m_replied = 0;
    m->m_data = 0;
    m->m_bufdata = 0;
    m->m_roffset = 0;
    m->m_total = 0;
    m->m_rmid = 0;
    m->m_vote = SILENT;
}


void grmem_init(g, m)
    group_p g;
    member_p m;
{
    m->m_state = M_UNUSED;
    m->m_expect = 1;
    m->m_messid = 0;
    m->m_retrial = g->g_maxretrial;
    m->m_replied = 0;
    m->m_data = 0;
    m->m_bufdata = 0;
    m->m_roffset = 0;
    m->m_total = 0;
    m->m_rmid = 0;
    m->m_vote = SILENT;
}
