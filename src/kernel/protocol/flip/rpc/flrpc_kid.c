/*	@(#)flrpc_kid.c	1.7	96/02/27 14:03:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "flrpc_kid.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "machdep.h"
#include "sys/proto.h"    

/* This module keeps track of transaction identifiers per kid */

#define LOGHASH		7
#define NHASH	     	(1 << LOGHASH)
#define hash(kid)	((kid)->u_long[0] & (NHASH - 1)) /* kids are random */

#define SWEEP_MSEC	(600L * 1000L)
#define SWEEP_SEC	SWEEP_MSEC / 1000

extern uint16 nkernel;
static kid_p freekid;
static kid_p kidtab[NHASH];
static int kid_npurge;


/* In principle, the kid table should never be purged, because the at-most-once
 * semantics of a RPC depends it. However, memory is not (yet) unlimited.
 * So, if the kid table is full we purge the oldest entry and hope that
 * it is an entry for a kernel that does not live anymore.
 */
static void kid_purge()
{
    int i;
    int h;
    kid_p kid;
    kid_p old = 0;
    kid_p p;

    kid_npurge++;
    for(i = 0; i < NHASH-1; i++) {
	kid = kidtab[i];
	while(kid != 0) {
	    if(old == 0 || kid->k_idle > old->k_idle) {
		old = kid;
	    }
	    kid = kid->k_next;
	}
    }

    assert(old);
    h = hash(&old->k_kid);
    assert(h >= 0 && h < NHASH);
    assert(kidtab[h]);
    for(kid = kidtab[h], p = 0; kid != 0; p = kid,
	kid = kid->k_next) {
	if(ADR_EQUAL(&kid->k_kid, &old->k_kid)) {
	    if(p == 0) {
		kidtab[h] = kid->k_next;
	    } else {
		p->k_next = kid->k_next;
	    }
	    kid->k_next = freekid;
	    freekid = kid;
	    break;
	}
    }
    assert(kid != 0);
}


/* Sweep through the kid table. */
/*ARGSUSED*/
static void kid_sweep(arg)
    long arg;
{
    int i;
    kid_p kid;

    for(i = 0; i < NHASH-1; i++) {
	kid = kidtab[i];
	while(kid != 0) {
	    kid->k_idle += SWEEP_SEC;
	    kid = kid->k_next;
	}
    }
}


/* Alloc kid's and start sweeper. */
void kid_init()
{
    kid_p k, kidlist;
    
    DPRINTF(0, ("kid_init: %d aalloc kid_t of size %d\n", nkernel, sizeof(kid_t)));
    kidlist = (kid_p) aalloc((vir_bytes)(sizeof(kid_t) * nkernel), 0);
    assert(kidlist);

    for(k = kidlist; k < kidlist + nkernel; k++) {
	k->k_next = freekid;
	freekid = k;
    }

    sweeper_set(kid_sweep, SWEEP_MSEC, SWEEP_MSEC, 0);
}


/* Look kid up in the hash table. */
kid_p kid_lookup(kid)
    adr_p kid;
{
    register kid_p k;
    int h = hash(kid); 
    
    assert(h >= 0 && h < NHASH);
    k = kidtab[h];
    while(k != 0) {
	if(ADR_EQUAL(kid, &k->k_kid)) {
	    k->k_idle = 0;
	    return(k);
	}
	k = k->k_next;
    }
    return(0);
}


/* Install a new kid in the hashtable. */
kid_p kid_install(kid, tsn)
    adr_p kid;
    uint32 tsn;
{
    register kid_p n;
    int h = hash(kid);
    register i;
    
    assert(h >= 0 && h < NHASH);

    if(freekid == 0) {
	kid_purge();
    }

    n = freekid;
    freekid = n->k_next;
    assert(n != 0);

    n->k_last = tsn;
    n->k_kid = *kid;
    n->k_index = 0;
    n->k_bitmap[0] = 1;
    for (i=1;i<TSN_BITMAP_SIZE;i++)
	n->k_bitmap[i] = 0;
    n->k_idle = 0;
    n->k_next = kidtab[h];
    kidtab[h] = n;
    return(n);
}


/* Update the kid with the last tsn. The kid entry remembers the last
 * TSN_WINDOW tsn's since k_last.
 */
void kid_update(kid, tsn)
    kid_p kid;
    uint32 tsn;
{
    int bit;

    kid->k_idle = 0;
    bit = tsn - kid->k_last;
    if(bit < 0) {
	return;
    }
    while(bit >= TSN_WINDOW) {
	bit -= 32;
	kid->k_last += 32;
	/* clear the oldest bitmap entry since that will become the newest */
	kid->k_bitmap[kid->k_index] = 0;
	kid->k_index = (kid->k_index+1)&TSN_BITMAP_MODULO;
    }
    SETBIT(kid, bit);
}


/* Is the rpc with (kid, tsn) still executable?
 * If not, return 0 and print an apropriate error message.
 */
int kid_execute(kid, tsn)
    kid_p kid;
    uint32 tsn;
{
    int bit;

    kid->k_idle = 0;
    if(tsn < kid->k_last) {	
	/* too old, be safe and make sure that the rpc is aborted  */
#ifndef SMALL_KERNEL
	printf("abort rpc: ");
	adr_print(&kid->k_kid); 
	printf(" tid %d last %d\n", tsn, kid->k_last);
#endif /* SMALL_KERNEL */
	return(0);
    }
    bit = tsn - kid->k_last;
    if(bit >= TSN_WINDOW) { /* very new? */
	return(1);
    }
    if (GETBIT(kid, bit)) {
	/* yes, abort the rpc */
#ifndef SMALL_KERNEL
	printf("abort rpc: ");
	adr_print(&kid->k_kid);
	printf(" tid %d last %d index %d\n", tsn, kid->k_last, kid->k_index);
	bitmap_dump((char *) 0, (char *) 0, kid->k_bitmap);
#endif /* SMALL_KERNEL */
	return(0);
    }
    return(1);
}

/* Same as kid_execute, only don't print error messages since it's not
 * always an error if the RPC has already been executed (this is the
 * case for RPC "enquire" messages).
 */
int kid_execute_silent(kid, tsn)
    kid_p kid;
    uint32 tsn;
{
    int bit;

    kid->k_idle = 0;
    if(tsn < kid->k_last) {	
	/* too old, be safe and make sure that the rpc is aborted  */
	return(0);
    }
    bit = tsn - kid->k_last;
    if(bit >= TSN_WINDOW) { /* very new? */
	return(1);
    }
    if (GETBIT(kid, bit)) {
	/* yes, abort the rpc */
	return(0);
    }
    return(1);
}


#ifndef SMALL_KERNEL
int bitmap_dump(begin, end, bitmap)
    char *begin, *end;
    uint32 *bitmap;
{
    int i;
    char *p = begin;
    
    for (i=0; i<TSN_BITMAP_SIZE; i+=4)
	p = bprintf(p, end, "\t%8x %8x %8x %8x\n",
		    bitmap[i+0], bitmap[i+1], bitmap[i+2], bitmap[i+3]);
    return(p - begin);
}


int kid_dump(begin, end)
    char *begin, *end;
{
    char *p;
    kid_p kid;
    int i;
    
    p = bprintf(begin, end, "================ kid dump =============\n");
    for(i = 0; i < NHASH-1; i++) {
	kid = kidtab[i];
	while(kid != 0) {
	    p = bprintf(p, end, "%d: ", i);
	    p += badr_print(p, end, &kid->k_kid);
	    p = bprintf(p, end, " last %d index %d idle %d(s) map:\n", 
			kid->k_last, kid->k_index, kid->k_idle);
	    p += bitmap_dump(p, end, kid->k_bitmap);
	    kid = kid->k_next;
	}
    }
    p = bprintf(p, end, "# kid purge(s): %d\n", kid_npurge);
    p = bprintf(p, end, "========================================\n");
    return(p - begin);
}
#endif
