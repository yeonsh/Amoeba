/*	@(#)segcache.c	1.4	96/02/27 10:18:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Routines to manipulate the segment (info) cache data structure.
   
   The segment cache is kept in an array in the host info structure,
   but after setup it is only accessed as a singly-linked list starting
   at p->hi_schead.  The first entry is the entry most recently added;
   when a new entry is added, the last entry is sacrificed.  When an entry
   is added that is already in the cache, it is simply moved to the front.
   
   It should be remembered that the segment cache as maintained here is only
   an approximation of reality; we don't get up-to-date information when
   a processor throws away a cached text segment.  Making the cache too big
   therefore will often assign bonuses for segments that have long been
   discarded; but a small cache will miss opportunities to assign bonuses.
   
   Ideally, we should be able to get the true contents of the segment cache
   from the processor...
*/

#include "runsvr.h"
#include "segcache.h"

/* Initialize the cache.  Call whenever the processor comes up */

void
setupsegcache(p)
	struct hostinfo *p;
{
	struct segcache *sc;
	
	memset((char *) p->hi_sc, 0, SEGCACHESIZE * sizeof(p->hi_sc[0]));
	for (sc = p->hi_schead = &p->hi_sc[SEGCACHESIZE-1]; sc > p->hi_sc; --sc)
		sc->sc_next = &sc[-1];
	sc->sc_next = NULL;
}

/* Find out which segment is the cacheable */

segment_d *
whichseg(pd)
	process_d *pd;
{
	int i;
	segment_d *sd;
	
	for (sd = PD_SD(pd), i = 0; i < pd->pd_nseg; ++sd, ++i) {
		if ((sd->sd_type & MAP_GROWMASK) == MAP_GROWNOT &&
			(sd->sd_type & MAP_PROTMASK) == MAP_READONLY &&
			!NULLPORT(&sd->sd_cap.cap_port) &&
			!NULLPORT(&sd->sd_cap.cap_priv.prv_random)) {
			if (debugging >= 2)
				printf("cacheable segment %d (len %ld)\n",
							i, sd->sd_len);
			return sd;
		}
	}
	
	return NULL;
}

/* Return the size of segment sd if it exists on host p, 0 if not */

long
checksegbonus(p, sd)
	struct hostinfo *p;
	segment_d *sd;
{
	register struct segcache *sc;
	
	sc = p->hi_schead;
	do {
		if (PORTCMP(&sc->sc_cap.cap_priv.prv_random,
				&sd->sd_cap.cap_priv.prv_random) &&
			memcmp((char *) &sc->sc_cap, (char *) &sd->sd_cap,
							CAPSIZE) == 0) {
			return sd->sd_len;
		}
	} while ((sc = sc->sc_next) != NULL);
	return 0;
}

/* Put segment sd at the head of the segment cache on processor p */

void
useseg(p, sd)
	struct hostinfo *p;
	segment_d *sd;
{
	register struct segcache *sc, **psc;
	
	psc = &p->hi_schead;
	while ((sc = *psc)->sc_next != NULL) {
		if (PORTCMP(&sc->sc_cap.cap_priv.prv_random,
				&sd->sd_cap.cap_priv.prv_random) &&
			memcmp((char *) &sc->sc_cap, (char *) &sd->sd_cap,
								CAPSIZE) == 0)
			break;
		psc = &sc->sc_next;
	}
	sc->sc_cap = sd->sd_cap;
	if (psc != &p->hi_schead) {
		*psc = sc->sc_next;
		sc->sc_next = p->hi_schead;
		p->hi_schead = sc;
	}
}
