/*	@(#)fclose.c	1.2	94/04/07 10:51:09 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fclose.c - flush a stream and close the file
 */

#include <stdio.h>
#include <stdlib.h>
#include "loc_incl.h"
#include "locking.h"

int close _ARGS((int d));

int
fclose(fp)
FILE *fp;
{
	register int i, retval = 0;
	int cnt;

	if (fflush(fp)) retval = EOF;

	LOCK(fp, cnt);

	for (i=0; i<FOPEN_MAX; i++)
		if (fp == __iotab[i]) {
			__iotab[i] = 0;
			break;
		}

	if (i >= FOPEN_MAX) {
		UNLOCK(fp, cnt);
		return EOF;
	}
	if (close(fileno(fp))) retval = EOF;
	if ( io_testflag(fp,_IOMYBUF) && fp->_buf )
		free((_VOIDSTAR)fp->_buf);

        /* We need to do the unlock in case anyone is waiting on the mutex
         * while we are freeing the FILE struct and thus destroying the mutex.
	 */
	UNLOCK(fp, cnt);
	if (fp != stdin && fp != stdout && fp != stderr)
		free((_VOIDSTAR)fp);

	return retval;
}
