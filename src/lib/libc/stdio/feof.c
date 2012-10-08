/*	@(#)feof.c	1.2	94/04/07 10:51:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * feof.c - test if eof on a stream occurred
 */

#include	<stdio.h>

#undef feof	/* but we still can use __feof */

int
feof(stream)
FILE *stream;
{
	return __feof(stream);
}
