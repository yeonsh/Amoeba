/*	@(#)atof.c	1.2	94/04/07 10:47:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NOFLOAT

#include <stdlib.h>
#include <errno.h>

#ifndef __STDC__
#define const /*const*/
#endif

double
atof(nptr)
const char *nptr;
{
        double d;
        int e = errno;

        d = strtod(nptr, (char **) NULL);
        errno = e;
        return d;
}

#else /* NOFLOAT*/

#ifndef lint
static int x;	/* to shut up ranlib and nm */
#endif

#endif
