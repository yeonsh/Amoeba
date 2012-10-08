/*	@(#)routtab.c	1.8	96/02/27 14:05:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This module maintains the routing table.  This table maps FLIP addresses,
 * location independent, to a list of sites defined by <network, location,
 * hopcount, age> tuples.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "routtab.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "machdep.h"
#include "sys/proto.h"

#define LOGHASH		   8	/* 2log size of hash table */

#define NHASH		(1 << LOGHASH)	/* size of hash table */
#define hash(adr)	((adr)->u_long[0] & (NHASH - 1))
#define MAX(a, b)	((a < b) ? b : a)

extern uint16 flip_nnetwork;
extern uint16 flip_maxntw;
extern uint16 adr_nadr;
extern uint16 adr_nntw;
extern uint16 adr_nloc;

/* Cache for the last src and dst seen (per network). Is not needed
 * for the correct functioning of the protocol, but it can improve
 * performance.
 */
route_p	route_cache;	

/* Free lists of adr, ntw, and loc structures. */
static struct adr_info *adr_info;	/* routing table */
static struct adr_info *hashtab[NHASH];	/* hash table for adr_info */
static struct adr_info *adr_free;	/* free list */

static struct ntw_info *ntw_info;
static struct ntw_info *ntw_free;

static struct loc_info *loc_info;
static struct loc_info *loc_free;
static location nullloc;

/* Info about routing table on which the clean up of the routing table is
 * based.
 */
static int route_npurge;	/* number of times that adr_purge is called */
static int route_ai_timedout;	/* number of adresses timed out by sweeper */
static int route_ni_npurge;	/* purged because of lack of ni structs */
static int route_ai_npurge;	/* purged because of lack of ai structs */
static int route_li_npurge;	/* purged because of lack of li structs */
static long route_tottime;	/* sum of the age fields of locations */ 
static int route_nloc;		/* number of locations in the routing table */
static int route_nadr;		/* number of addresses in the routing table */
static long route_oldest;	/* age of the oldest location */

static void adr_purge_oldest _ARGS((adr_p keep)); /* forward */

int adr_random(adr)
    adr_p adr;
{
    uint16 *p = (uint16 *) adr;
    uint16 rand();

    *p++ = rand();
    *p++ = rand();
    *p++ = rand();
    *p++ = rand();
    adr->a_space = 0;
}


/*
 * Debug routines.
 */
    
#ifndef SMALL_KERNEL

int badr_print(begin, end, adr)
    char *begin, *end;
    adr_p adr;
{
    register i;
    char *p;
    
    p = begin;
    
    for (i = 0; i < sizeof(adr->a_abytes); i++)
	p = bprintf(p, end, "%x:", adr->a_abytes[i] & 0xFF);
    p = bprintf(p, end, "%x", adr->a_space & 0xFF);
    return(p-begin);
}


int bloc_print(begin, end, loc)
    char *begin, *end;
    location *loc;
{
    register i;
    char *p = begin;
    
    p = bprintf(p, end, "%x", loc->l_bytes[0] & 0xFF);
    for (i = 1; i < sizeof(loc->l_bytes); i++)
	p = bprintf(p, end, ":%x", loc->l_bytes[i] & 0xFF);
    return(p-begin);
}


static int li_dump(begin, end, li)
    char *begin, *end;
    struct loc_info *li;
{
    char *p = begin;
    
    while (li != 0) {
	p = bprintf(p, end, "\t");
	p += bloc_print(p, end, &li->li_location);
	if (li->li_local)
		p = bprintf(p, end, " local %d", li->li_local);
	if (li->li_hopcnt)
		p = bprintf(p, end, " hopcnt %d", li->li_hopcnt);
	if (li->li_idle)
		p = bprintf(p, end, " age %d", li->li_idle);
	if (li->li_count)
		p = bprintf(p, end, " #loc %d", li->li_count);
	if (li->li_trust_count != li->li_count)
		p = bprintf(p, end, " #tloc %d", li->li_trust_count);
	if (li->li_mcast)
		p = bprintf(p, end, " mcast %d", li->li_mcast);
	p = bprintf(p, end, "\n");
	li = li->li_next;
    }
    return(p-begin);
}


static int ni_dump(begin, end, ni)
    char *begin, *end;
    struct ntw_info *ni;
{
    char *p = begin;
    
    while (ni != 0) {
	p = bprintf(p, end, "    %d (", ni->ni_network);
	p = bprintf(p, end,"#loc %d", ni->ni_count);
	if (ni->ni_trust_count != ni->ni_count)
		p = bprintf(p, end,"#tloc %d", ni->ni_trust_count);
	if (ni->ni_nmcast) {
		p = bprintf(p, end, "(multi: ");
		p += bloc_print(p, end, &ni->ni_multicast);
		p = bprintf(p, end, ",#mcast %d)", ni->ni_nmcast);
	}
	p = bprintf(p, end, ") -\n");
	p += li_dump(p, end, ni->ni_loc);
	ni = ni->ni_next;
    }
    return(p-begin);
}


static int ai_dump(begin, end, ai)
    char *begin, *end;
    struct adr_info *ai;
{
    char *p = begin;
    
    while (ai != 0) {
	p = bprintf(p, end, "%d: ", hash(&ai->ai_address));
	p += badr_print(p, end, &ai->ai_address);
	p = bprintf(p, end, " #loc %d", ai->ai_count);
	if (ai->ai_trust_count != ai->ai_count)
		p = bprintf(p, end, " #tloc %d", ai->ai_trust_count);
	p = bprintf(p, end, " hopcnt %d - \n", ai->ai_hopcnt);
	p += ni_dump(p, end, ai->ai_ntw);
	ai = ai->ai_next;
    }
    return(p - begin);
}


int adr_dump(begin, end)
    char *begin, *end;
{
    struct adr_info **pai;
    char *p = begin;
    
    p = bprintf(p, end, "================ routing table =============\n");
    p = bprintf(p, end, 
	"nadr %d; remote: nloc %d oldest %d average %d purges: %d\n", 
	route_nadr, route_nloc, route_oldest, route_tottime / (route_nloc ?
		route_nloc : 1), route_npurge);
    p = bprintf(p, end, 
		"ai_timedout: %d, ai_purge: %d; li_purge: %d; ni_purge: %d\n", 
		route_ai_timedout, route_ai_npurge, route_ni_npurge,
		route_li_npurge);
    for (pai = hashtab; pai < &hashtab[NHASH]; pai++)
	p += ai_dump(p, end, *pai);
    p = bprintf(p, end, "============================================\n");
    return(p - begin);
}
#endif


/* Invalidate adr in the route_cache. */
static void route_invalidate(adr)
    adr_p adr;
{
    route_p r;

#if PRINTF_LEVEL >= 2
    DPRINTF(2, ("route_invalidate: "));
    badr_print((char *) 0, (char *) 0, adr);
    DPRINTF(2, ("\n"));
#endif
    int_invalidate(adr);
    for(r=route_cache; r < route_cache + flip_nnetwork; r++) {
	if(r->r_sadr != 0 && ADR_EQUAL(adr, &(r->r_sadr->ai_address))) {
	    r->r_sadr = 0;
	}
	if(r->r_dadr != 0 && ADR_EQUAL(adr, &(r->r_dadr->ai_address))) {
	    r->r_dadr = 0;
	}
    }
}


#define THRESHOLD	(((int) adr_nadr * 3) >> 2)

/* This routine is called at regular intervals called sweeps.  At each
 * sweep all idle counters are incremented. If the routing table gets
 * ``full'', throw out all entries older than the average age. In this way
 * adr_purge_oldest() is hopefully never called and routes are remembered as
 * long as possible.
 */
/*ARGSUSED*/
static void
adr_sweep(id)
    long id;
{
    struct adr_info **pai;
    struct adr_info *ai;
    struct ntw_info **pni;
    struct ntw_info *ni;
    struct loc_info **pli;
    struct loc_info *li;
    long average;
    int purge = 0;
    int i;
    
    if(route_nadr > THRESHOLD) {
	average = route_tottime / route_nloc;
	purge = 1;
    }
    route_nloc = 0;
    route_nadr = 0;
    route_tottime = 0;
    route_oldest = 0;

    for (i = 0; i < NHASH; i++) {
	pai = &hashtab[i];
	while ((ai = *pai) != 0) {
	    pni = &ai->ai_ntw;
	    while ((ni = *pni) != 0) {
		pli = &ni->ni_loc;
		while ((li = *pli) != 0) {
		    if (purge && li->li_idle >= average && !li->li_local) {
			/* remove location */
			route_ai_timedout++;
			DPRINTF(2, ("adr_sweep: "));
			route_invalidate(&ai->ai_address);
			if (li->li_mcast == LI_MCAST) ni->ni_nmcast -= li->li_count;
			ni->ni_trust_count -= li->li_trust_count;
			ai->ai_trust_count -= li->li_trust_count;
			ni->ni_count -= li->li_count;
			ai->ai_count -= li->li_count;
			compare(ni->ni_count, >=, ni->ni_trust_count);
			compare(ai->ai_count, >=, ai->ai_trust_count);
			*pli = li->li_next;
			li->li_next = loc_free;
			loc_free = li;
		    } else {
			/* don't add id to avoid overflowing route_tottime */
		        li->li_idle++;
			if(!li->li_local) {
		    	    route_tottime += li->li_idle;
	    		    route_nloc++;
		    	    if(route_oldest < li->li_idle) 
				route_oldest = li->li_idle;
			}
			pli = &li->li_next;
		    }
		}
		if (ni->ni_loc == 0) {
#ifndef NOMULTICAST
		    flip_delmcast(&ai->ai_address, ni, NONTW);
#endif
		    *pni = ni->ni_next;
		    ni->ni_next = ntw_free;
		    ntw_free = ni;
		}
		else
		    pni = &ni->ni_next;
	    }
	    if (ai->ai_ntw == 0) {
		*pai = ai->ai_next;
		ai->ai_next = adr_free;
		adr_free = ai;
	    } else {
		route_nadr++;
		pai = &ai->ai_next;
            }
	}
    }
}


/* Alloc routines.
 */

static struct loc_info *li_init(loc, local, hopcnt, count, trusted_count)
    location *loc;
    int local;
    f_hopcnt_t hopcnt;
    uint16 count;
    uint16 trusted_count;
{
    struct loc_info *li;
    
    if ((li = loc_free) == 0) {
	DPRINTF(0, ("li_init: out of location structures\n"));
	return 0;
    }
    loc_free = li->li_next;
    li->li_idle = 0;
    li->li_mcast = 0;
    li->li_location = *loc;
    li->li_local = local;
    li->li_hopcnt = hopcnt;
    li->li_count = count;
    li->li_trust_count = trusted_count;
    return li;
}


static struct ntw_info *ni_init(ntw)
    int ntw;
{
    struct ntw_info *ni;
    
    if ((ni = ntw_free) == 0) {
	DPRINTF(0, ("ni_init: out of network structures\n"));
	return 0;
    }
    ntw_free = ni->ni_next;
    ni->ni_multicast = nullloc;
    ni->ni_nmcast = 0;
    ni->ni_count = 0;
    ni->ni_trust_count = 0;
    ni->ni_network = ntw;
    return ni;
}


static struct adr_info *ai_alloc(){
    struct adr_info *ai;
    
    if ((ai = adr_free) == 0) {
	DPRINTF(0, ("ai_alloc: out of address structures\n"));
	return 0;
    }
    ai->ai_count = 0;
    ai->ai_trust_count = 0;
    ai->ai_hopcnt = 0;
    adr_free = ai->ai_next;
    return ai;
}


static int li_add(pli, loc, hopcnt, count, trusted_count, local)
    struct loc_info **pli;
    location *loc;
    f_hopcnt_t hopcnt;
    uint16 *count;
    uint16 *trusted_count;
    int local;
{
    register struct loc_info *li;
    int old_count = 0;
    int old_trusted_count = 0;
    
    compare(*count, >=, *trusted_count);

    for (li = *pli; li != 0; li = li->li_next) {
	if (LOC_EQUAL(&li->li_location, loc)) {
	    break;
	}
    }

    if (li == 0) {
	if ((li = li_init(loc, local, hopcnt, *count, *trusted_count)) == 0)
	    return -1;
	li->li_next = *pli;
	*pli = li;
    } else { 
	old_count = li->li_count;
	old_trusted_count = li->li_trust_count;
	if (hopcnt != li->li_hopcnt) {
	    li->li_hopcnt = hopcnt;
	}
	if(!li->li_local) li->li_local = local;
	li->li_count = *count;
	li->li_trust_count = *trusted_count;
	li->li_idle = 0;
    }

    *count = li->li_count - old_count;
    *trusted_count = li->li_trust_count - old_trusted_count;
    
    compare(li->li_count, >=, li->li_trust_count);

    return 0;
}


static int ni_add(pni, ntw, loc, hopcnt, count, trusted_count, local)
    struct ntw_info **pni;
    int ntw;
    location *loc;
    f_hopcnt_t hopcnt;
    uint16 *count;
    uint16 *trusted_count;
    int local;
{
    register struct ntw_info *ni;

    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    for (ni = *pni; ni != 0; ni = ni->ni_next)
	if (ni->ni_network == ntw)
	    break;

    if (ni == 0) {
	if ((ni = ni_init(ntw)) == 0)
	    return -1;
	ni->ni_next = *pni;
	*pni = ni;
    }
    if(li_add(&ni->ni_loc, loc, hopcnt, count, trusted_count, local) != -1) {
	ni->ni_count += *count;
	ni->ni_trust_count += *trusted_count;
	compare(ni->ni_count, >=, ni->ni_trust_count);
	return 0;
    } else {
	return -1;
    }
}


static int ai_add(pai, adr, ntw, loc, hopcnt, count, trusted_count, local)
    struct adr_info **pai;
    adr_p adr;
    int ntw;
    location *loc;
    f_hopcnt_t hopcnt;
    uint16 count;
    uint16 trusted_count;
    int local;
{
    struct adr_info *ai;
    
    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    for (ai = *pai; ai != 0; ai = ai->ai_next)
	if (ADR_EQUAL(&ai->ai_address, adr))
	    break;
    if (ai == 0) {		/* there was no entry */
	if ((ai = ai_alloc()) == 0)
	    return -1;
	ai->ai_address = *adr;
	ai->ai_next = *pai;
	*pai = ai;
    }
    ai->ai_hopcnt = MAX(ai->ai_hopcnt, hopcnt);
    if(ni_add(&ai->ai_ntw, ntw, loc, hopcnt, &count, &trusted_count, local) !=
       -1) {
	ai->ai_count += count;
	ai->ai_trust_count += trusted_count;
	compare(ai->ai_count, >=, ai->ai_trust_count);
	return 0;
    } else {
	return -1;
    }
}


/* Install an address and its physical location in the routing table.
 * First check whether the address is already there.  If so, its contents
 * is updated with the latest information.  The idle timer is reset.
 */
#ifdef __STDC__
void adr_add(adr_p adr, int ntw, location *loc, f_hopcnt_t hopcnt,
				uint16 count, uint16 trusted_count, int local)
#else
void adr_add(adr, ntw, loc, hopcnt, count, trusted_count, local)
    adr_p adr;
    int ntw;
    location *loc;
    f_hopcnt_t hopcnt;
    uint16 count;
    uint16 trusted_count;
    int local;
#endif
{
    struct adr_info **pai;

    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    assert(!ADR_NULL(adr));
    pai = &hashtab[hash(adr)];
    if (ai_add(pai, adr, ntw, loc, hopcnt, count, trusted_count, local) == -1) {
        route_ai_npurge++;
	adr_purge_oldest(adr);
	if (ai_add(pai, adr, ntw, loc, hopcnt, count, trusted_count, local) ==
	    -1) {
	    panic("internal error in adr_add");
	}
    }
}


/* Install an address and its physical location in the routing table.
 * First check whether the address is already there.  If so, its contents
 * is updated with the latest information.  The idle timer is reset.
 */
#ifdef __STDC__
void adr_install(adr_p adr, int ntw, location *loc, f_hopcnt_t hopcnt,
						    int local, int trusted)
#else
void adr_install(adr, ntw, loc, hopcnt, local, trusted)
    adr_p adr;
    int ntw;
    location *loc;
    f_hopcnt_t hopcnt;
    int local;
    int trusted;
#endif
{
    struct adr_info **pai;
    register struct adr_info *ai;
    register struct ntw_info *ni = 0;
    register struct loc_info *li = 0;
    register route_p r = route_cache+ntw;
    
    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    if(ADR_NULL(adr)) return;
    assert(!LOC_NULL(loc));
    pai = &hashtab[hash(adr)];
    for (ai = *pai; ai != 0; ai = ai->ai_next)
	if (ADR_EQUAL(&ai->ai_address, adr)) {
	    for (ni = ai->ai_ntw; ni != 0; ni = ni->ni_next) {
		if (ni->ni_network == ntw) {
		    for (li = ni->ni_loc; li != 0; li = li->li_next) {
			if (LOC_EQUAL(&li->li_location, loc)) {
			    if (hopcnt != li->li_hopcnt) {
				li->li_hopcnt = hopcnt;
				ai->ai_hopcnt = MAX(ai->ai_hopcnt, hopcnt);
			    }
			    li->li_idle = 0;
			    if(!li->li_local) li->li_local = local;
			    if(li->li_trust_count > 0 && !trusted) {
		             printf("warning: adr_install trusted changed\n");
			    }
			    r->r_sadr = ai;
			    r->r_sloc = li;
			    return;
			}
		    }
		    break;
		}
	    }
	    break;
	}
    DPRINTF(2, ("adr_install: "));
    route_invalidate(adr);
    if (ai == 0) {		/* there was no entry */
	if ((ai = ai_alloc()) == 0) {
	    /* purge table, freeing at least one ai */
	    route_ai_npurge++;
	    adr_purge_oldest(adr);
	    if ((ai = ai_alloc()) == 0) {
		/* shouldn't happen now */
		printf("adr_install: cannot allocate new adr_info\n");
		return;
	    }
	}
	ai->ai_address = *adr;
	ai->ai_next = *pai;
	*pai = ai;
    }
    if (ni == 0) {
	if ((ni = ni_init(ntw)) == 0) {
	    /* purge table, freeing at least one ni */
	    route_ni_npurge++;
	    adr_purge_oldest(adr);
	    if ((ni = ni_init(ntw)) == 0) {
		/* shouldn't happen now */
		printf("adr_install: cannot allocate new ntw_info\n");
		return;
	    }
	}
	ni->ni_next = ai->ai_ntw;
	ai->ai_ntw = ni;
    }
    if (li == 0) {
	uint16 ntrusted = trusted ? 1 : 0;

	if ((li = li_init(loc, local, hopcnt, 1, ntrusted)) == 0) {
	    /* purge table, freeing at least one li */
	    route_li_npurge++;
	    adr_purge_oldest(adr);
	    if ((li = li_init(loc, local, hopcnt, 1, ntrusted)) == 0) {
		/* shouldn't happen now */
		printf("adr_install: cannot allocate new loc_info\n");
		return;
	    }
	}
	li->li_next = ni->ni_loc;
	ni->ni_loc = li;
	ni->ni_count++;
	ai->ai_count++;
	if(trusted) {
	    ni->ni_trust_count++;
	    ai->ai_trust_count++;
	}
	ai->ai_hopcnt = MAX(ai->ai_hopcnt, hopcnt);
	compare(li->li_count, >=, li->li_trust_count);
	compare(ni->ni_count, >=, ni->ni_trust_count);
	compare(ai->ai_count, >=, ai->ai_trust_count);
    }
    li->li_idle = 0;
    r->r_sadr = ai;
    r->r_sloc = li;
}


/* Find the address in the routing table, and return a pointer to the
 * entry.
 */
struct ntw_info *adr_lookup(adr, count, hopcnt, safe)
    adr_p adr;
    uint16 *count;
    f_hopcnt_t *hopcnt;
    int safe;
{
    register struct adr_info *ai;
    
    if (ADR_NULL(adr)) return NI_BROADCAST;
    for (ai = hashtab[hash(adr)]; ai != 0; ai = ai->ai_next)
	if (ADR_EQUAL(&ai->ai_address, adr)) {
	    if(count != 0) *count = safe ? ai->ai_trust_count : ai->ai_count;
	    if(hopcnt != 0) *hopcnt = ai->ai_hopcnt;
#ifdef RVDP_DEBUG
	    if ((int)(ai->ai_ntw) & 3) {
		printf("unaligned address in hashtable\n");
		return 0;
	    }
#endif
	    return ai->ai_ntw;
	}
    return 0;
}


uint16 adr_count(adr, hopcnt, notntw, trusted)
    adr_p adr;
    f_hopcnt_t *hopcnt;
    int notntw;
    int trusted;
{
    register struct ntw_info *ni, *nilist;
    register struct loc_info *li;
    register uint16 count = 0;
    f_hopcnt_t hcnt;
    
    assert(!ADR_NULL(adr));
    if ((nilist = adr_lookup(adr, (uint16 *) 0, (f_hopcnt_t *) 0, trusted)) ==
	0) {
	return 0;
    }
    if (hopcnt != 0) {	
	hcnt = 0;
	for(ni = nilist; ni != 0; ni = ni->ni_next) {
	    if (ni->ni_network != notntw) {
		for(li = ni->ni_loc; li != 0; li = li->li_next) {
		    hcnt = MAX(hcnt, li->li_hopcnt);
		}
	    }
	}
	*hopcnt = hcnt;
    }
    for(ni = nilist; ni != 0; ni = ni->ni_next) {
	if (ni->ni_network != notntw) {
	    count += trusted ? ni->ni_trust_count : ni->ni_count;
	}
    }
    return count;
}


static uint16 li_remove(pli, loc, all, ni_ntrust)
    struct loc_info **pli;
    location *loc;
    int all;
    uint16 *ni_ntrust;
{
    struct loc_info *li;
    uint16 count = 0;
    
    *ni_ntrust = 0;
    while ((li = *pli) != 0) {
	if ((LOC_NULL(loc) || LOC_EQUAL(&li->li_location, loc)) &&
	    (!li->li_local || all)) {
	    compare(li->li_count, >=, li->li_trust_count);
	    *pli = li->li_next;
	    count += li->li_count;
	    *ni_ntrust += li->li_trust_count;
	    li->li_next = loc_free;
	    loc_free = li;
	} else pli = &li->li_next;
    }
    return(count);
}


static uint16 ni_remove(adr, pni, ntw, loc, all, nontw, ntrust)
    adr_p adr;
    struct ntw_info **pni;
    location *loc;
    int ntw, all, nontw;
    uint16 *ntrust;
{
    struct ntw_info *ni;
    uint16 count = 0;
    uint16 n;
    uint16 ni_ntrust;
    
    *ntrust = 0;
    while ((ni = *pni) != 0) {
	if ((ntw == NONTW && ni->ni_network != nontw) || 
	                                         ni->ni_network == ntw) {
	    n = li_remove(&ni->ni_loc, loc, all, &ni_ntrust);
	    count += n;
	    ni->ni_count -= n;
	    *ntrust += ni_ntrust;
	    ni->ni_trust_count -= ni_ntrust;
	    compare(ni->ni_count, >=, ni->ni_trust_count);
	    if (ni->ni_loc == 0) {
#ifndef NOMULTICAST
		if(adr != (adr_p) 0) 
		    flip_delmcast(adr, ni, nontw);
#endif
		*pni = ni->ni_next;
		ni->ni_next = ntw_free;
		ntw_free = ni;
	    } else pni = &ni->ni_next;
	} else pni = &ni->ni_next;
    }
    return(count);
}


static void ai_remove(pai, adr, ntw, loc, all, nontw)
    struct adr_info **pai;
    adr_p adr;
    location *loc;
    int ntw, all, nontw;
{
    struct adr_info *ai;
    uint16 ntrust;

    while ((ai = *pai) != 0) {
	if (adr == 0 || ADR_EQUAL(&ai->ai_address, adr)) {
	    ai->ai_count -= ni_remove(adr, &ai->ai_ntw, ntw, loc, all, nontw,
				      &ntrust);
	    ai->ai_trust_count -= ntrust;
	    compare(ai->ai_count, >=, ai->ai_trust_count);
	    if (ai->ai_ntw == 0) {
		*pai = ai->ai_next;
		ai->ai_next = adr_free;
		adr_free = ai;
	    } else pai = &ai->ai_next;
	} else pai = &ai->ai_next;
    }
}



/* Remove entries from the routing table.  The entries should have adr and
 * ntw as their values.  If ntw is NONTW, the ntw doesn't matter.  If loc
 * is 0, all these entries should be removed.  If loc is not 0, then only
 * that entry conforming to that location should be removed. If nontw is 
 * different from NONTW, only entries not located on nontw are removed.
 */
void adr_remove(adr, ntw, loc, all, nontw)
    adr_p adr;
    location *loc;
    int ntw, all, nontw;
{
    DPRINTF(2, ("adr_remove: "));
    route_invalidate(adr);
    ai_remove(&hashtab[hash(adr)], adr, ntw, loc, all, nontw);
}


/* Purge the entire routing table.  This is only needed when the network
 * structure is modified by means of flip_control.
 */
void adr_purge()
{
    struct adr_info **pai;
    
    route_npurge++;
    for (pai = hashtab; pai < &hashtab[NHASH]; pai++)
	ai_remove(pai, (adr_p) 0, NONTW, &nullloc, 0, NONTW);
}

static void
adr_purge_oldest(keep)
adr_p keep;
{
    /* Remove the least recently used address from the routing tables,
     * making room for a new entry.  Skip the entry with address 'keep'.
     * One would hope that normally the sweeper would take care of removing
     * the oldest entries soon enough.  However, it is possible that many
     * new addresses are generated in a short period of time (e.g. when a
     * group with many members is created) so we need something to fall back
     * on in case the heuristic fails.
     */
    struct adr_info *ai, *oldest_ai, **oldest_hash;
    struct ntw_info *ni;
    struct loc_info *li;
    long oldest_idle, this_idle;
    int is_local;
    int i;
    
    oldest_ai = 0;
    oldest_hash = 0;
    oldest_idle = -1;

    /* Find address that has the highest idle time. */
    for (i = 0; i < NHASH; i++) {
	for (ai = hashtab[i]; ai != 0; ai = ai->ai_next) {
	    /* Among the locations for this address,
	     * use the idle time of the one most recently used.
	     */
	    this_idle = -1;
	    is_local = 0;
	    for (ni = ai->ai_ntw; ni != 0; ni = ni->ni_next) {
		for (li = ni->ni_loc; li != 0; li = li->li_next) {
		    if (li->li_local) {
			/* don't purge an address with a local location */
			is_local = 1;
			break;
		    } else {
			if (this_idle < 0) {
			    this_idle = li->li_idle;
			} else {
			    if (li->li_idle < this_idle) {
				this_idle = li->li_idle;
			    }
			}
		    }
		}
	    }

	    if (!is_local && this_idle > oldest_idle &&
		!ADR_EQUAL(&ai->ai_address, keep))
	    {
		oldest_ai = ai;
		oldest_hash = &hashtab[i];
		oldest_idle = this_idle;
	    }
	}
    }
		
    if (oldest_ai != 0) {
	adr_t oldest_addr;

	oldest_addr.u_addr = oldest_ai->ai_address.u_addr;
	DPRINTF(2, ("adr_purge: "));
	route_invalidate(&oldest_addr);
	ai_remove(oldest_hash, &oldest_addr, NONTW, &nullloc, 0, NONTW);
    } else {
	/* shouldn't happen */
	printf("adr_purge_oldest: cannot purge address\n");
    }
}

/* Pick one of the sites associated with rt that is not on notntw and
 * within maxhops.  Heuristic:  pick the closest one.
 */
#ifdef __STDC__
int adr_route(adr_p adr, f_hopcnt_t maxhops, int *dstntw, location *dstloc,
				f_hopcnt_t *hopcnt, int notntw, int secure)
#else
int adr_route(adr, maxhops, dstntw, dstloc, hopcnt, notntw, secure)
    adr_p adr;
    f_hopcnt_t maxhops;
    int *dstntw;
    location *dstloc;
    f_hopcnt_t *hopcnt;
    int notntw;
    int secure;
#endif
{
    register struct adr_info *ai;
    register struct ntw_info *ni;
    register struct loc_info *li;
    register struct loc_info *best_li;
    register route_p r = 0;
    int found;
    f_hopcnt_t minhopcnt;
    
    if (ADR_NULL(adr)) return(ADR_TOOFARAWAY);
    found = ADR_TOOFARAWAY;
    minhopcnt  = maxhops;
    if(notntw != NONTW) {
	assert(notntw >= 0 && notntw < (int) flip_nnetwork);
	r = route_cache + notntw;
    }
    for (ai = hashtab[hash(adr)]; ai != 0; ai = ai->ai_next) {
	if (ADR_EQUAL(&ai->ai_address, adr)) {
	    ni = ai->ai_ntw;
	    while (ni != 0) {
		if(ni->ni_network != notntw) {
		    li = ni->ni_loc;
		    while(li != 0) {
			if(secure && li->li_trust_count <= 0 && found ==
			   ADR_TOOFARAWAY) { 
			    found = ADR_UNSAFE;
			}
			if(li->li_hopcnt <= minhopcnt && (secure ? 
					     li->li_trust_count > 0  : 1)) {
			    minhopcnt = li->li_hopcnt;
			    best_li = li;
			    if (dstntw != 0) 
				*dstntw = ni->ni_network;
			    if (dstloc != 0) 
				*dstloc = best_li->li_location;
			    if (hopcnt != 0) 
				*hopcnt = best_li->li_hopcnt;
			    if(r != 0) {
				r->r_dadr = ai;
				r->r_dloc = li;
				r->r_dntw = ni;
				r->r_dhcnt = li->li_hopcnt;
			    }
			    found = ADR_OK;
			}
			li = li->li_next;
		    }
		}
		ni = ni->ni_next;
	    }
	    return found;
	}
    }
    return(found);
}

/* Set up the free list of routing table entries.
 */
void
adr_init(){
    struct adr_info *ai;
    struct ntw_info *ni;
    struct loc_info *li;
    route_p r;
    
    DPRINTF(0, ("adr_init: aalloc %d adr_info of size %d\n",
					adr_nadr, sizeof(struct adr_info)));
    adr_info = (struct adr_info *)
		    aalloc((vir_bytes)(sizeof(struct adr_info)*adr_nadr), 0);
    DPRINTF(0, ("adr_init: aalloc %d ntw_info of size %d\n",
					adr_nntw, sizeof(struct ntw_info)));
    ntw_info = (struct ntw_info *)
		    aalloc((vir_bytes)(sizeof(struct ntw_info)*adr_nntw), 0);
    DPRINTF(0, ("adr_init: aalloc %d loc_info of size %d\n",
					adr_nloc, sizeof(struct loc_info)));
    loc_info = (struct loc_info *)
		    aalloc((vir_bytes)(sizeof(struct loc_info)*adr_nloc), 0);
    DPRINTF(0, ("adr_init: aalloc %d cache-route\n", flip_maxntw));
    route_cache = (route_p)
			aalloc((vir_bytes)(sizeof(route_t) * flip_maxntw), 0);
    for(r = route_cache; r < route_cache + flip_maxntw; r++) {
	r->r_sadr = 0;
	r->r_dadr = 0;
    }
    for (ai = adr_info; ai < &adr_info[adr_nadr]; ai++) {
	ai->ai_next = adr_free;
	adr_free = ai;
    }
    for (ni = ntw_info; ni < &ntw_info[adr_nntw]; ni++) {
	ni->ni_next = ntw_free;
	ntw_free = ni;
    }
    for (li = loc_info; li < &loc_info[adr_nloc]; li++) {
	li->li_next = loc_free;
	loc_free = li;
    }
    sweeper_set(adr_sweep, 25000L, 25000L, 0);
}
