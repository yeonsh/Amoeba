/*	@(#)ip_host_lkup.c	1.1	96/02/27 11:01:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	ip_host_lookup
**
**	This routine tries to find the capability for the specified
**	IP server component on the specified host.
**
**	Author: Gregory J. Sharp, Sept 1995
*/

#include <stdio.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "string.h"
#include "module/name.h"
#include "module/host.h"


#define	MAX_PATH_NAME_LENGTH	128

errstat
ip_host_lookup(hostname, ext, cap)
char *		hostname;	/* in: name of host to look up */
char *		ext;		/* in: IP svr component to look up */
capability *	cap;		/* filled in: capability for host processor */
{
    char	buf[MAX_PATH_NAME_LENGTH];
    errstat	err;
    
    if (strlen(ext) + strlen(hostname) > MAX_PATH_NAME_LENGTH - 5)
	return STD_ARGBAD;

    (void) sprintf(buf, "%s/ip/%s", hostname, ext);
    err = host_lookup(buf, cap);
    if (err != STD_OK)
    {
	/* Maybe the hostname was already a full path to the server? */
	err = name_lookup(hostname, cap);
    }
    return err;
}
