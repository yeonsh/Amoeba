/*	@(#)hostlk_sup.c	1.2	94/04/07 10:08:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	super_host_lookup
**
**	This routine tries to find the capability for the specified host.
**	If it is an absolute pathname it does a straight lookup.
**	If it is a relative pathname it looks it up under HOST_SUPER_DIR and
**	if that fails it looks under the current directory.
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
super_host_lookup(hostname, cap)
char *		hostname;	/* in: name of host to look up */
capability *	cap;		/* filled in: capability for host processor */
{
    char	buf[MAX_PATH_NAME_LENGTH];
    
    if (*hostname == '/')
	return name_lookup(hostname, cap);
    if (*hostname == '.' && *(hostname + 1) == '/')
	return name_lookup(hostname + 2, cap);
    
#ifdef UNIX_HOST_DIR
/* On Unix, first try not using the directory server */
    if (strlen(UNIX_HOST_DIR) + 1 + strlen(hostname) <= MAX_PATH_NAME_LENGTH) {
    	int fd;
    	(void) sprintf(buf, "%s/%s", UNIX_HOST_DIR, hostname);
    	if ((fd = open(buf, 0)) >= 0) {
    	    int n;
    	    n = read(fd, (char *) cap, CAPSIZE);
    	    close(fd);
    	    if (n == CAPSIZE)
    	        return STD_OK;
    	}
    }
#endif
    
    /* Make sure we don't overflow our buffer */
    if (strlen(HOST_SUPER_DIR) + 1 + strlen(hostname) > MAX_PATH_NAME_LENGTH)
	return STD_NOMEM;

    (void) sprintf(buf, "%s/%s", HOST_SUPER_DIR, hostname);
    return name_lookup(buf, cap);
}
