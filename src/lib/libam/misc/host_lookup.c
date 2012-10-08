/*	@(#)host_lookup.c	1.4	94/04/07 10:07:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**	host_lookup
**
**	This routine tries to find the capability for the specified host.
**	It starts by trying to see if the user has a super capability.
**	If it is an absolute pathname it does a straight lookup.
**	If it is a relative pathname it looks it up using super_host_lookup
**	and if that fails it looks under HOST_DIR and then the current
**	directory.
**	The value of cap is only valid if the function returns STD_OK.
**	No effort is made to trap illegal parameters.  On good systems they
**	will generate bus errors.
*/

#include <stdio.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "ampolicy.h"
#include "string.h"
#include "module/name.h"
#include "module/host.h"

#ifdef UNIX
#include "amupolicy.h"
#ifndef UNIX_HOST_DIR
#define UNIX_HOST_DIR		"."
#endif
#endif

#define	MAX_PATH_NAME_LENGTH	256

errstat
host_lookup(hostname, cap)
char *		hostname;	/* in: name of host to look up */
capability *	cap;		/* filled in: capability for host processor */
{
    char	buf[MAX_PATH_NAME_LENGTH];
    errstat	err;
    
    if ((err = super_host_lookup(hostname, cap)) != STD_OK)
    {
	/*
	 * Look it up in HOST_DIR - may have less rights but at least
	 * we might find a capability.
	 *
	 * Make sure we don't overflow our buffer.
	 */
	if (strlen(HOST_DIR) + 1 + strlen(hostname) > MAX_PATH_NAME_LENGTH)
	    return STD_NOMEM;

	(void) sprintf(buf, "%s/%s", HOST_DIR, hostname);
	err = name_lookup(buf, cap);

	/* Otherwise look it up in the current directory */
	if (err != STD_OK)
	    err = name_lookup(hostname, cap);
    }
    return err;
}
