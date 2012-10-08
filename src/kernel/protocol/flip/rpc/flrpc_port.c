/*	@(#)flrpc_port.c	1.7	96/02/27 14:04:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#ifdef SPEED_HACK1
#include "../sys/routtab.h"
#endif
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "sys/flip/flrpc_port.h"
#include "machdep.h"
#include "sys/proto.h"
    
/* This module implements port management. */
    
extern uint16 rpc_maxcache;

#ifdef SPEED_HACK1
extern f_hopcnt_t max_hops;
#endif

#define LOGHASH		7
#define MAXHASH		(1 << LOGHASH)
#define HASHMASK	(MAXHASH -1)
#define SWEEP		20000L

#define hash(p)		((* (int *) (p)) & HASHMASK)

#define MON_EVENT(str)	DPRINTF(0, ("%s\n", str));


static portcache_p hashtab[MAXHASH];
static portcache_p freecache;
static int port_npurge;
static port nullport;
static long port_tottime;	/* sum of the age fields of ports */ 
static int port_nport;		/* number of ports in the cache */
static long port_oldest;	/* age of the oldest port */

#define THRESHOLD	((3 * (int) rpc_maxcache) >> 2)

/* Sweep through the port cache and remove old entries for remote servers. */
/*ARGSUSED*/
static void port_sweep(val)
    long val;
{
    int j;
    portcache_p p, c;
    int purge = 0;
    long average;

    if(port_nport >= THRESHOLD) {
	average = port_tottime / port_nport;
	purge = 1;
    }
    port_nport = 0;
    port_tottime = 0;
    port_oldest = 0;
    
    for(j=0 ; j < MAXHASH; j++) {
	for(p = 0, c = hashtab[j]; c != 0; ) {
	    /* Increment idle time (don't add val to avoid overflow when
	     * computing port_tottime).
	     */
	    c->p_idle++;
	    if(purge && c->p_where != P_LOCAL && c->p_idle >= average) {
		/* a non local old port; remove it. */
		if(p == 0) {
		    hashtab[j] = c->p_next;
		} else {
		    p->p_next = c->p_next;
		}
		c->p_pubport = nullport;
		c->p_next = freecache;
		freecache = c;

		if(p == 0) c = hashtab[j];
		else c = p->p_next;
	    } else {
		if(c->p_where != P_LOCAL) {
		    port_nport++;
		    port_tottime += c->p_idle;
		    if(c->p_idle > port_oldest)
			port_oldest = c->p_idle;
		}
		p = c;
		c = c->p_next;
	    }
	}
    }
}


void port_init()
{
    portcache_p p;

    DPRINTF(0, ("port_init: aalloc %d cache entries of size %d\n",
			rpc_maxcache, sizeof(portcache_t)));
    freecache = (portcache_p)
		    aalloc((vir_bytes)(sizeof(portcache_t) * rpc_maxcache), 0);

    for(p = freecache; p < &freecache[rpc_maxcache-1]; p++) {
	p->p_next = p+1;
	p->p_pubport = nullport;
    }
    p->p_next = 0;
    
    sweeper_set(port_sweep, SWEEP, SWEEP, 0);
}


#ifdef __STDC__
void port_remove(port *prt, adr_p adr, uint16 thread, int owner)
#else
void port_remove(prt, adr, thread, owner)
    port *prt;
    adr_p adr;
    uint16 thread;
    int owner;
#endif
{
    portcache_p c, p;
    int h = hash(prt);
    
    assert(h >= 0 && h < MAXHASH);
    for(p = 0, c = hashtab[h]; c != 0; p = c, c = p->p_next) {
	if(PORTCMP(prt, &c->p_pubport) && ADR_EQUAL(&c->p_addr, adr) && 
	   ((owner && c->p_thread == thread) || (c->p_where != P_LOCAL))) {
	    if(hashtab[h] == c) {
		hashtab[h] = c->p_next;
	    } else {
		p->p_next = c->p_next;
	    }
	    c->p_pubport = nullport;
	    c->p_next = freecache;
	    freecache = c;
	    return;
	}
    }
}


/* Try to remove a port from the portcache. This hurts performance. If this
 * happens a lot, you might want to increase rpc_maxcache. If there are
 * more local server threads than rpc_maxcache, panic.
 */
static void port_purge()
{
    portcache_p c, old;
    int h;


    port_npurge++;
    old = 0;
    for(h = 0; h < MAXHASH; h++) {
	for(c = hashtab[h]; c != 0; c = c->p_next) {
	    if((old == 0 || old->p_idle < c->p_idle) && c->p_where != P_LOCAL) 
	    { 
		old = c;
	    }
	}
    }
    assert(old);
    port_remove(&old->p_pubport, &old->p_addr, 0, 0);
    assert(freecache);
}


/* Install port in the port cache. */
#ifdef __STDC__
void port_install(uint16 thread, adr_p a, port *p, int where,
					    interval roundtrip, char *arg)
#else
void port_install(thread, a, p, where, roundtrip, arg)
    uint16 thread;
    adr_p a;
    port *p;
    int where;
    interval roundtrip;
    char *arg;
#endif
{
    register int h = hash(p);
    register portcache_p c;

    assert(h >= 0 && h < MAXHASH);
    /* See if the port is already in the cache. */
    for(c = hashtab[h]; c != 0; c = c->p_next) {
	if(PORTCMP(p, &c->p_pubport) && ADR_EQUAL(a, &c->p_addr) && thread ==
	   c->p_thread) {	/* is it there? */
	    c->p_roundtrip = roundtrip;
	    c->p_where = where;
	    c->p_idle = 0;
	    c->p_arg = arg;
	    return;
	}
    }

    /* Port is not present in the cache, enter it. */
    if(freecache == 0) {
	port_purge();
    }
    c = freecache;
    freecache = c->p_next;

    c->p_next = hashtab[h];
    hashtab[h] = c;

    c->p_pubport = *p;
    c->p_where = where;
    c->p_roundtrip = roundtrip;
    c->p_thread = thread;
    c->p_addr = *a;
    c->p_idle = 0;
    c->p_arg = arg;
#ifdef SPEED_HACK1
    if(c->p_where != P_LOCAL && adr_route(a, max_hops, &c->p_ntw, &c->p_loc,
					  &c->p_hopcnt, NONTW, SECURE)!= ADR_OK)
    { 
	c->p_hopcnt = max_hops;
    }
#endif
}


/* Find an address (process) that listens to prt. */
portcache_p port_lookup(prt)
    port *prt;
{
    register portcache_p c;
    register int h = hash(prt);

    assert(h >= 0 && h < MAXHASH);
    for(c = hashtab[h]; c != 0; c = c->p_next) {
	if(PORTCMP(prt, &c->p_pubport)) {
	    c->p_idle = 0;
	    return(c);
	}
    }
    return(0);
}


/* Find a local thread that belong to process adr and listens to port prt.
 * If there exists one, remove it from the port cache.
 */
int port_match(prt, adr, thread)
    port *prt;
    adr_p adr;
    uint16 *thread;
{
    register portcache_p c, p;
    register int h = hash(prt);
    
    assert(h >= 0 && h < MAXHASH);
    for(p = 0, c = hashtab[h]; c != 0; p = c, c = p->p_next) {
	if(PORTCMP(prt, &c->p_pubport) && c->p_where == P_LOCAL &&
	    ADR_EQUAL(adr, &c->p_addr)) {
	    if(hashtab[h] == c) {
		hashtab[h] = c->p_next;
	    } else {
		p->p_next = c->p_next;
	    }
	    assert(thread != 0);
	    *thread = c->p_thread;
	    c->p_pubport = nullport;

	    c->p_next = freecache;
	    freecache = c;
	    return(1);
	}
    }
    return(0);
}


#ifndef SMALL_KERNEL
int port_dump(begin, end)
    char *begin, *end;
{
    char *p;
    portcache_p c;
    int j;
    
    p = bprintf(begin, end, "================ port dump =============\n");
    for(j=0 ; j < MAXHASH; j++) {
	for(c = hashtab[j]; c != 0; c = c->p_next) {
	    p = bprintf(p, end, "%d: public %P : ", j, &c->p_pubport);
	    p += badr_print(p, end, &c->p_addr);
	    p = bprintf(p, end, 
			" round-trip %d thread: %d\n\twhere: %s age: %d\n", 
			c->p_roundtrip, c->p_thread, (c->p_where == P_LOCAL) ?
			"local" : "remote", c->p_idle); 
	}
    }
    p = bprintf(p, end, "# purge %d; remote: nport %d oldest %d average %d\n",
		port_npurge, port_nport, port_oldest, (port_nport > 0) ? 
		port_tottime / port_nport : 0); 
    p = bprintf(p, end, "========================================\n");
    return(p - begin);
}
#endif
