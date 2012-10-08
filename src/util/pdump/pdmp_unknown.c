/*	@(#)pdmp_unknown.c	1.3	96/02/27 13:10:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** Dump a cluster descriptor with unknown architecture.
** (This is really impossible if we don't know the byte order;
** assume the same as the current machine.)
*/

#include "stdio.h"

#include "amoeba.h"
#include "server/process/proc.h"


void
unknown_dumpfault(fd, fdlen)
    char *fd;
    int  fdlen;
{
    int i;
    
    fprintf(stdout, "\tFault descriptor (%d bytes):", fdlen);
    for (i = 0; i < fdlen; i++) {
	if (i % 16 == 0)
	    fprintf(stdout, "\n\t\t");
	fprintf(stdout, "%2.2x ", fd[i] & 0xff);
    }
    fprintf(stdout, "\n");
}
