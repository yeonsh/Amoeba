/*	@(#)flcache.c	1.3	94/04/06 08:46:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This is the cache module.  It is used to check whether packets have
 * been transmitted recently.  If this is the case, there's probably
 * a loop in the network topology, and the packet switch will have to
 * make appropriate actions.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"

#include "assert.h"
INIT_ASSERT

#define NCACHE		(1024)
#define HASHMASK	(NCACHE - 1)

typedef struct flcache { 
    adr_t	c_adr;
    long	c_messid;
    long	c_offset;
} *cache_p, cache_t;

static cache_t cache[NCACHE];

/* See whether the packet defined by it's FLIP header has been sent
 * recently.  We calculate a hash value by adding the appropriate fields
 * and masking it with HASHMASK. If the header ``matches'' with the values
 * stored in the cache, the packets is looping. If not, store the values,
 * so that if it arrives again, the loop is detected.
 * It may happen, that we detect some loops not; for example, two interleaved
 * looping packets.
 */

flcache_present(fh)
    flip_hdr *fh;
{
    long hash = 0;
    cache_t *c;
    
    hash += fh->fh_srcaddr.u_long[0];
    hash += fh->fh_srcaddr.u_long[1];
    hash += fh->fh_messid;
    hash += fh->fh_offset;
    
    c = cache + (hash & HASHMASK);
    assert(c >= cache && c < cache + NCACHE);
    
    if(ADR_EQUAL(&c->c_adr, &fh->fh_srcaddr) && c->c_messid == fh->fh_messid
       && c->c_offset == fh->fh_offset) {
	return 1;
    }

    c->c_adr = fh->fh_srcaddr;
    c->c_messid = fh->fh_messid;
    c->c_offset = fh->fh_offset;

    return 0;
}
