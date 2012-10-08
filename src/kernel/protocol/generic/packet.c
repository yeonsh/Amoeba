/*	@(#)packet.c	1.6	96/02/27 14:06:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This is the packet management module.  A described in packet.h, packets
 * can partly reside in user space or on local memory of the network
 * device.  This module maintains a pool of packet buffers, and
 * chooses the interface for the packet switch when it wants to send a
 * packet.
 */

#include <string.h>
#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/proto.h"
#ifdef UNIX
#include <stdio.h>
#endif
#include "assert.h"
INIT_ASSERT

#ifdef UNIX
#define NO_FLIP_WAIT_SUPPORT
#endif

#ifdef notdef
extern pool_t flip_pool;


/* Return a packet from FLIP's pool.
 */
pkt_p pkt_acquire(){
    pkt_p pkt;
    
    PKT_GET(pkt, &flip_pool);
    return(pkt);
}

/* Return a duplicate of pkt by acquiring a new one and copying the
 * contents.
 */
pkt_p pkt_new(pkt)
    pkt_p pkt;
{
    pkt_p new;
    
    if ((new = pkt_acquire()) != 0) {
	pkt_copy(pkt, new);
    }
    return new;
}
#endif

pkt_copy(pkt1, pkt2) 
    register pkt_p pkt1, pkt2;
{
    register char *p, *q; 
    
    assert(pkt1);
    assert(pkt2);
    assert(pkt1->p_admin.pa_allocated); 
    assert(pkt2->p_admin.pa_allocated); 
    assert(pkt1->p_admin.pa_header.ha_type == HAT_ABSENT); 
    assert(pkt1->p_contents.pc_dirsize + pkt1->p_contents.pc_offset <=
							pkt2->p_admin.pa_size);
#ifndef NO_FLIP_WAIT_SUPPORT
    assert(pkt2->p_admin.pa_wait == 0);
    if ((pkt2->p_admin.pa_wait = pkt1->p_admin.pa_wait) != 0) {
	/* increase refcount of waiting caller */
	++(*pkt2->p_admin.pa_wait);
    }
#endif
    pkt2->p_contents.pc_dirsize = pkt1->p_contents.pc_dirsize; 
    pkt2->p_contents.pc_offset = pkt1->p_contents.pc_offset; 
    pkt2->p_contents.pc_totsize = pkt1->p_contents.pc_totsize; 
    pkt2->p_contents.pc_dstype = pkt1->p_contents.pc_dstype; 
    pkt2->p_contents.pc_dsident = pkt1->p_contents.pc_dsident; 
    pkt2->p_contents.pc_virtual = pkt1->p_contents.pc_virtual; 
    p = pkt_offset(pkt1);
    q = pkt_offset(pkt2);
    (void) memmove((_VOIDSTAR) q, (_VOIDSTAR) p,
		   (size_t) pkt1->p_contents.pc_dirsize);
}


pkt_pullup(pkt, size)
    pkt_p pkt;
{
    register int s = size - pkt->p_contents.pc_dirsize;
    
    assert(pkt->p_contents.pc_dstype == 0);
    assert(pkt->p_contents.pc_offset + s + pkt->p_contents.pc_dirsize
	   <= pkt->p_admin.pa_size);
    (void) memmove((_VOIDSTAR) (pkt_offset(pkt) + pkt->p_contents.pc_dirsize),
		   (_VOIDSTAR) pkt->p_contents.pc_virtual, (size_t) s);
    pkt->p_contents.pc_virtual += s;
    pkt->p_contents.pc_dirsize += s;
}


void pkt_discard(pkt)
    register pkt_p pkt;
{
    register pool_p pool;
    pkt_p hpkt;
    
    assert(pkt->p_admin.pa_allocated);
    if (pkt->p_admin.pa_release != 0) {
	(*pkt->p_admin.pa_release)(pkt->p_admin.pa_arg);
	pkt->p_admin.pa_release = 0;
    }
#ifndef NO_FLIP_WAIT_SUPPORT
    if (pkt->p_admin.pa_wait != 0) {
	compare(*pkt->p_admin.pa_wait, >, 0);
	if (--(*pkt->p_admin.pa_wait) == 0) {
	    /* last copy of the packet gone; wakeup sender */
	    wakeup((event) pkt->p_admin.pa_wait);
	}
	pkt->p_admin.pa_wait = 0;
    }
#endif
    pkt->p_admin.pa_header.ha_type = HAT_ABSENT;
    assert(pkt->p_admin.pa_pool != 0);
    assert(pkt->p_admin.pa_allocated);
    pkt->p_admin.pa_allocated = 0;
    pool = pkt->p_admin.pa_pool;
    pkt->p_admin.pa_next = pool->pp_freelist;
    pkt->p_admin.pa_priority = 0;
    pkt->p_contents.pc_dstype = 0;
    pkt->p_contents.pc_dsident = 0;
    pool->pp_nbuf++;
    assert(pool->pp_nbuf <= pool->pp_maxbuf);
    pool->pp_freelist = pkt;
    while (pool->pp_wanted > 0 && pool->pp_freelist != 0) {
	PKT_GET(hpkt, pool);
	assert(hpkt);
	pool->pp_wanted--;
	assert(pool->pp_wanted >= 0);
	assert(pool->pp_notify != 0);
	(*pool->pp_notify)(pool->pp_arg, hpkt);
    } 
}


pkt_setwanted(pool, n)
    register pool_p pool;
    int n;
{
    register pkt_p pkt;
    
    (pool)->pp_wanted = (n); 
    while ((pool)->pp_wanted > 0 && (pool)->pp_freelist != 0) { 
	PKT_GET(pkt, pool); 
	(pool)->pp_wanted--; 
	assert((pool)->pp_wanted >= 0); 
	assert((pool)->pp_notify != 0);
	(*(pool)->pp_notify)((pool)->pp_arg, pkt); 
    } 
}


pkt_incwanted(pool, n)
    register pool_p pool;
    int n;
{
    register pkt_p pkt;
    
    assert((pool)->pp_wanted >= 0);
    (pool)->pp_wanted += (n); 
    while ((pool)->pp_wanted > 0 && (pool)->pp_freelist != 0) { 
	PKT_GET(pkt, pool); 
	(pool)->pp_wanted--; 
	assert((pool)->pp_wanted >= 0); 
	assert((pool)->pp_notify != 0);
	(*(pool)->pp_notify)((pool)->pp_arg, pkt); 
    } 
}

static pool_p	pool_list;

void pkt_init(pool, size, bufs, nbufs, data, notify, arg)
    pool_p pool;
    int size;
    pkt_p bufs;
    int nbufs;
    char *data;
    void (*notify)();
    long arg;
{
    register i;
    pkt_p b;
    
    assert(!data || size >= PKTENDHDR);

    pool->pp_next = pool_list;
    pool_list = pool;

    pool->pp_freelist = 0;
    pool->pp_flags = 0;
    pool->pp_wanted = 0;
    pool->pp_notify = notify;
    pool->pp_arg = arg;
    pool->pp_maxbuf = nbufs;
    pool->pp_nbuf = 0;
    for (b = bufs, i = 0; b < bufs + nbufs; i++, b++) {
	b->p_admin.pa_pool = pool;
	b->p_admin.pa_allocated = 1;
	b->p_admin.pa_size = size;
	b->p_admin.pa_release = 0;
#ifndef NO_FLIP_WAIT_SUPPORT
	b->p_admin.pa_wait = 0;
#endif
	b->p_contents.pc_buffer = data + size * i;
	pkt_discard(b);
    }
}

#ifndef SMALL_KERNEL
pooldebug(begin, end)
char *begin, *end;
{
	char *p;
	pool_p pp;

	p = bprintf(begin, end, "Packet pool debugging:\n");
	p = bprintf(p, end, "pool      nbuf\tmaxbuf\tflags\n");
	for (pp = pool_list; pp; pp = pp->pp_next) {
		p = bprintf(p, end, "%8x  %d\t%d\t0x%x\n",
			pp, pp->pp_nbuf, pp->pp_maxbuf, pp->pp_flags);
	}
	return p - begin;
}
#endif

#ifdef ROMKERNEL
pkt_p func_pkt_get(pool)
pool_p pool;
{
	register pkt_p pkt;

	MACRO_PKT_GET(pkt, pool);
	return pkt;
}
#endif
