/*	@(#)ferror.c	1.2	94/04/07 10:51:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * ferror .c - test if an error on a stream occurred
 */

#include	<stdio.h>

#undef ferror	/* but we still can use __ferror */

int
ferror(stream)
FILE *stream;
{
	return __ferror(stream);
}
