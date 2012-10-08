/*	@(#)badport.c	1.2	94/04/06 11:37:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "monitor.h"
#include "stderr.h"
#include "cmdreg.h"
#include "svr.h"

/*
 *	To remember ports that aren't serviced.
 *
 *	Call capsaid() to report the error status of any stub,
 *	badport() to guess whether we can use a capability,
 *	and reset_badport() to forget them.
 */

#ifndef MAX_BAD_PORT	/* How many ports we can remember */
#define MAX_BAD_PORT	20
#endif

static port bad_ports[MAX_BAD_PORT];
static int n_badports = 0;
static mutex mu;
extern int debugging;
extern capability *outcap;

errstat
capsaid(cap, err)
    capability *cap;
    errstat err;
{
    mu_lock(&mu);
    if (n_badports < SIZEOFTABLE(bad_ports) && err == RPC_NOTFOUND) {
	int i;
	for (i = 0; i < n_badports; ++i)
	    if (PORTCMP(&cap->cap_port, &bad_ports[i])) {
		MON_EVENT("Found bad port twice");
		MU_CHECK(mu);
		mu_unlock(&mu);
		return err;
	    }
	MON_EVENT("Found a bad port");
	bad_ports[n_badports++] = cap->cap_port;
    }
    MU_CHECK(mu);
    mu_unlock(&mu);
    return err;
} /* capsaid */

int
badport(cap)
    capability *cap;
{
    register int i;
    if (n_badports == 0) return 0;	/* Common case */
    mu_lock(&mu);
    for (i = n_badports; --i >= 0 && !PORTCMP(&cap->cap_port, &bad_ports[i]); )
	;
    MU_CHECK(mu);
    mu_unlock(&mu);
    return i >= 0;
} /* badport */

void
reset_badport()
{
    MON_EVENT("Reset bad ports");
    n_badports = 0;
} /* reset_badport */

void
list_badport()
{
    if (badport(outcap)) return;	/* Would deadlock */
    if (n_badports)
	prf("%n%d bad ports\n", n_badports);
} /* list_badport */
