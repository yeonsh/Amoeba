/*	@(#)fgetpos.c	1.2	94/04/07 10:51:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fgetpos.c - get the position in the file
 */

#include	<stdio.h>

int
fgetpos(stream, pos)
FILE *stream;
fpos_t *pos;
{
	*pos = ftell(stream);
	if (*pos == -1) return -1;
	return 0;
}
