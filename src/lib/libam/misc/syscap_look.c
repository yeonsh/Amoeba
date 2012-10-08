/*	@(#)syscap_look.c	1.3	94/04/07 10:08:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * SYSCAP_LOOKUP
 *
 * This routine tries to look up the sys server cap for the given host.
 * Normally this is in the DEF_SYSSVRNAME entry in a kernel directory.
 * However if the kernel is a ROM kernel then in has no kernel directory
 * and the sys server listens to the kernel port, but with a different
 * check field and rights from the kernel directory capability.
 * If it can't get the host from /super/hosts then it tries /profile/hosts.
 *
 * Author:
 *	Gregory J. Sharp, Sept 1991.
 * Modified:
 *	Gregory J. Sharp, Nov 1993 - converted to use super_host_lookup as
 *				     part of kernel security upgrade.
 */

#include "amoeba.h"
#include "ampolicy.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/direct.h"
#include "module/host.h"

errstat
syscap_lookup(host, cap)
char *		host;	/* in: name of host as accepted by host_lookup */
capability *	cap;	/* out: capability for sys server */
{
    errstat	err;
    capability	mcap;	/* capability for the host in question */

    if ((err = host_lookup(host, &mcap)) == STD_OK &&
	(err = dir_lookup(&mcap, DEF_SYSSVRNAME, cap)) == STD_COMBAD)
    {
	    /* Return the host cap - it's a ROM kernel */
	    *cap = mcap;
	    err = STD_OK;
    }
    return err;
}
