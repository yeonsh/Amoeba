/*	@(#)clearerr.c	1.2	94/04/07 10:50:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * clearerr.c - clear error and end-of-file indicators of a stream
 */

#include	<stdio.h>

#undef clearerr	/* but we still can use __clearerr */

void
clearerr(stream)
FILE *stream;
{
	__clearerr(stream);
}
