/*	@(#)flgrp_marsh.c	1.2	96/02/27 14:01:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h" 
#include "debug.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "group.h"
#include "flgrp_header.h"
#include "flgrp_hist.h"
#include "flgrp_type.h"
#include "flgrp_member.h"
#include "flgrp_marsh.h"
#include "flgrp_hstpro.h"

#include "byteorder.h"
#include "grp_endian.h"
#include "../rpc/am_endian.h"
#include "string.h"
#include "module/fliprand.h"


/* Couple of marshalling routines for group information.
 * We could have used the buf_*() routines, but the byteorder is known,
 * and the integers are all properly aligned.
 */

static char *
grp_unmarshall_adr(p, end, adr)
char *p;
char *end;
adr_p adr;
{
    if (p == NULL || (end - p) < sizeof(adr_t)) {
	return NULL;
    }
    *adr = * (adr_p) p;
    return (p + sizeof(adr_t));
}

char *
grp_unmarshall_uint16(p, end, i)
char   *p;
char   *end;
uint16 *i;
{
    if (p == NULL || (end - p) < sizeof(uint16)) {
	return NULL;
    }
    *i = * (uint16 *) p;
    return (p + sizeof(uint16));
}

char *
grp_unmarshall_uint32(p, end, i)
char   *p;
char   *end;
uint32 *i;
{
    if (p == NULL || (end - p) < sizeof(uint32)) {
	return NULL;
    }
    *i = * (uint32 *) p;
    return (p + sizeof(uint32));
}

static char *
grp_marshall_adr(p, end, adr)
char *p;
char *end;
adr_p adr;
{
    if (p == NULL || (end - p) < sizeof(adr_t)) {
	return NULL;
    }
    * (adr_p) p = *adr;
    return (p + sizeof(adr_t));
}

char *
grp_marshall_uint16(p, end, i)
char   *p;
char   *end;
uint16 *i;
{
    if (p == NULL || (end - p) < sizeof(uint16)) {
	return NULL;
    }
    * (uint16 *) p = *i;
    return (p + sizeof(uint16));
}

static char *
grp_marshall_uint32(p, end, i)
char   *p;
char   *end;
uint32 *i;
{
    if (p == NULL || (end - p) < sizeof(uint32)) {
	return NULL;
    }
    * (uint32 *) p = *i;
    return (p + sizeof(uint32));
}


char *
grp_marshall_result(buf, end, g, hdr)
char     *buf;
char     *end;
group_p   g;
header_p  hdr;
{
    int	  i;
    char *p;
    
    p = buf;
    if ((end - p) < sizeof(header)) {
	return NULL;
    }
    (void) memmove((_VOIDSTAR) p, (_VOIDSTAR) hdr, sizeof(header));
    p += sizeof(header);
    for (i = 0; i < (int) g->g_maxgroup; i++) {
	p = grp_marshall_uint32(p, end, &g->g_member[i].m_state);
    }
    p = grp_marshall_adr   (p, end, &g->g_prvaddr);
    p = grp_marshall_adr   (p, end, &g->g_prvaddr_all);
    p = grp_marshall_uint16(p, end, &g->g_total);
    p = grp_marshall_uint16(p, end, &g->g_seqid);
    p = grp_marshall_uint16(p, end, &g->g_incarnation);
    return p;
}


char *
grp_unmarshall_result(buf, end, bigendian, g)
char    *buf;
char    *end;
int      bigendian;
group_p  g;
{
    int		i;
    g_index_t	dum1;
    g_seqcnt_t	dum2;
    f_size_t	dum3;
    char       *p;

#ifdef lint
    dum1 = dum2 = dum3 = 0;
#endif
    
    p = buf;
    for (i = 0; i < (int) g->g_maxgroup; i++) {
	p = grp_unmarshall_uint32(p, end, &g->g_member[i].m_state);
	bc_orderint(bigendian, g->g_member[i].m_state);
    }
    p = grp_unmarshall_adr   (p, end, &g->g_prvaddr);
    p = grp_unmarshall_adr   (p, end, &g->g_prvaddr_all);
    p = grp_unmarshall_uint16(p, end, &g->g_total);
    p = grp_unmarshall_uint16(p, end, &g->g_seqid);
    p = grp_unmarshall_uint16(p, end, &g->g_incarnation);
    if (p != NULL) {
	bc_ordergroup(bigendian, g->g_total, g->g_seqid, dum2,
		      g->g_incarnation, dum1, dum3);
	g->g_seq = &g->g_member[g->g_seqid];
    }
    return p;
}


#ifdef __STDC__
char *grp_marshallgroup(char *buf, char *end,
			group_p g, g_index_t new, header_p hdr)
#else
char *grp_marshallgroup(buf, end, g, new, hdr)
char     *buf;
char     *end;
group_p   g;
g_index_t new;
header_p  hdr;
#endif
{
    int i;
    char *p;
    
    p = buf;
    if ((end - p) < sizeof(header)) {
	return NULL;
    }
    (void) memmove((_VOIDSTAR) p, (_VOIDSTAR) hdr, sizeof(header));
    p += sizeof(header);
    p = grp_marshall_adr(p, end, &g->g_member[new].m_addr);
    for (i = 0; i < (int) g->g_maxgroup; i++) {
	p = grp_marshall_adr   (p, end, &g->g_member[i].m_addr);
	p = grp_marshall_adr   (p, end, &g->g_member[i].m_rpcaddr);
	p = grp_marshall_uint32(p, end, &g->g_member[i].m_state);
	p = grp_marshall_uint32(p, end, &g->g_member[i].m_expect);
	p = grp_marshall_uint32(p, end, &g->g_member[i].m_messid);
    }
    p = grp_marshall_adr   (p, end, &g->g_prvaddr_all);
    p = grp_marshall_uint16(p, end, &g->g_total);
    p = grp_marshall_uint16(p, end, &g->g_seqid);
    p = grp_marshall_uint32(p, end, &g->g_nextseqno);
    p = grp_marshall_uint16(p, end, &g->g_incarnation);
    p = grp_marshall_uint16(p, end, &g->g_resilience);
    p = grp_marshall_uint32(p, end, &g->g_large);
    return p;
}

f_size_t
grp_max_marshall_size(g)
group_p g;
{
    return sizeof(header) + sizeof(adr_t) +
	   g->g_maxgroup * (2 * sizeof(adr_t) + 3 * sizeof(uint32)) +
	   sizeof(adr_t) + 4 * sizeof(uint16) + 2 * sizeof(uint32);
}

char *
grp_unmarshallgroup(buf, end, bigendian, g)
char   *buf;
char   *end;
int     bigendian;
group_p g;
{
    adr_t      mem_addr, mem_rpc_addr;
    uint32     mem_state;
    g_seqcnt_t mem_expect;
    g_msgcnt_t mem_messid;
    member_p  m;
    char     *p;

    p = buf;
    for (m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	p = grp_unmarshall_adr   (p, end, &mem_addr);
	p = grp_unmarshall_adr   (p, end, &mem_rpc_addr);
	p = grp_unmarshall_uint32(p, end, &mem_state);
	p = grp_unmarshall_uint32(p, end, &mem_expect);
	p = grp_unmarshall_uint32(p, end, &mem_messid);
	if (p != NULL) {
	    grmem_new(g, (g_index_t) (m - g->g_member),
		      &mem_addr, &mem_rpc_addr, 0L, 0L);
	    bc_ordermember(bigendian, mem_state, mem_expect, mem_messid);
	    m->m_state = mem_state;
	    m->m_expect = mem_expect;
	    m->m_messid = mem_messid;
	    GDEBUG(3, printf("grp_unmarshallgroup: %d: mess %d\n",
			     m - g->g_member, m->m_messid));
	}
    }
    p = grp_unmarshall_adr   (p, end, &g->g_prvaddr_all);
    p = grp_unmarshall_uint16(p, end, &g->g_total);
    p = grp_unmarshall_uint16(p, end, &g->g_seqid);
    p = grp_unmarshall_uint32(p, end, &g->g_nextseqno);
    p = grp_unmarshall_uint16(p, end, &g->g_incarnation);
    p = grp_unmarshall_uint16(p, end, &g->g_resilience);
    p = grp_unmarshall_uint32(p, end, &g->g_large);
    if (p != NULL) {
	flip_oneway(&g->g_prvaddr_all, &g->g_addr_all);
	bc_ordergroup(bigendian, g->g_total, g->g_seqid, g->g_nextseqno,
		      g->g_incarnation, g->g_resilience, g->g_large);
	g->g_seq = &g->g_member[g->g_seqid];
    }
    return p;
}

char *
grp_unmarshallmem(buf, end, bigendian, g, mem)
char    *buf;
char    *end;
int	 bigendian;
group_p  g;
member_p mem;
{
    adr_t      mem_addr, mem_rpc_addr;
    uint32     mem_state;
    g_seqcnt_t mem_expect;
    g_msgcnt_t mem_messid;
    member_p   m;
    char      *p;
    
    /* only update the state of the member indicated */
    p = buf;
    for (m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	p = grp_unmarshall_adr   (p, end, &mem_addr);
	p = grp_unmarshall_adr   (p, end, &mem_rpc_addr);
	p = grp_unmarshall_uint32(p, end, &mem_state);
	p = grp_unmarshall_uint32(p, end, &mem_expect);
	p = grp_unmarshall_uint32(p, end, &mem_messid);
	if (p != NULL && m == mem) {
	    grmem_new(g, (g_index_t) (m - g->g_member),
		      &mem_addr, &mem_rpc_addr, 0L, 0L);
	    bc_ordermember(bigendian, mem_state, mem_expect, mem_messid);
	    /* don't set m->m_state */
	    m->m_expect = mem_expect;
	    m->m_messid = mem_messid;
	    GDEBUG(3, printf("grp_unmarshalmem: %d: mess %d\n",
			     m - g->g_member, m->m_messid));
	}
    }
    return p;
}
