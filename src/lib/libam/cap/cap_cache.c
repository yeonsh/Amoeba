/*	@(#)cap_cache.c	1.3	96/02/27 10:59:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "monitor.h"
#include "module/mutex.h"
#include "stdlib.h"
#include "module/cap.h"
#include "module/prv.h"

/* The cache is a little bit tricky, since users can make capabilities
 * with identical server port, object number, and rights, but different
 * random numbers.
 */
static struct cap_cache {
	capability cc_cap;
	long cc_mask;
	port cc_check;
	struct cap_cache *cc_junior;
	struct cap_cache *cc_senior;
} *cc_cache, *cc_oldest, *cc_youngest, *cc_free;
static mutex cc_mutex;

int
cc_restrict(cap, mask, new, restrict)
capability *new, *cap;
rights_bits mask;
int (*restrict)();
{
	struct cap_cache *cc;
	register n;

	mask &= (cap->cap_priv.prv_rights & PRV_ALL_RIGHTS);

	/* if all rights are there, just copy the capability */
	if (mask == (cap->cap_priv.prv_rights & PRV_ALL_RIGHTS)) {
		MON_EVENT("trivial cap generation");
		*new = *cap;
		return 0;
	}

	/* look for it in the cache */
	mu_lock(&cc_mutex);
	for (cc = cc_youngest; cc != 0; cc = cc->cc_senior)
		if (mask == cc->cc_mask && cap_cmp(&cc->cc_cap, cap)) {
			MON_EVENT("cap cache hit");
			*new = *cap;
			new->cap_priv.prv_rights = mask;
			new->cap_priv.prv_random = cc->cc_check;
			if (cc->cc_junior != 0) {	/* update lru */
				if ((cc->cc_junior->cc_senior = cc->cc_senior)
									== 0)
					cc_oldest = cc->cc_junior;
				else
					cc->cc_senior->cc_junior =
								cc->cc_junior;
				cc_youngest->cc_junior = cc;
				cc->cc_senior = cc_youngest;
				cc->cc_junior = 0;
				cc_youngest = cc;
			}
			mu_unlock(&cc_mutex);
			return 0;
		}
	
	MON_EVENT("cap cache miss");
	if ((n = (*restrict)(cap, mask, new)) == 0) {
		/* now put it in the cache */
		if ((cc = cc_free) != 0)
			cc_free = cc->cc_senior;
		else {
			cc = cc_oldest;
			cc_oldest = cc->cc_junior;
			cc_oldest->cc_senior = 0;
		}
		if ((cc->cc_senior = cc_youngest) == 0)
			cc_oldest = cc;
		else
			cc_youngest->cc_junior = cc;
		cc->cc_cap = *cap;
		cc->cc_mask = mask;
		cc->cc_check = new->cap_priv.prv_random;
		cc->cc_junior = 0;
		cc_youngest = cc;
	}
	mu_unlock(&cc_mutex);
	return n;
}

int
cc_init(ncache)
int ncache;
{
	struct cap_cache *cc;
	static cc_initialized;

	mu_lock(&cc_mutex);
	if (!cc_initialized) {
		cc_cache = (struct cap_cache *)
				calloc((unsigned int) ncache, sizeof(*cc));
		if (cc_cache != 0) {
			for (cc = cc_cache; cc < &cc_cache[ncache]; cc++) {
				cc->cc_senior = cc_free;
				cc_free = cc;
			}
			cc_initialized = 1;
		}
	}
	mu_unlock(&cc_mutex);
	return cc_initialized;
}
