/*	@(#)rewind.c	1.2	94/04/07 10:55:01 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * rewind.c - set the file position indicator of a stream to the start
 */

#include	<stdio.h>
#include	"loc_incl.h"

void
rewind(stream)
FILE *stream;
{
	(void) fseek(stream, 0L, SEEK_SET);
	clearerr(stream);
}
