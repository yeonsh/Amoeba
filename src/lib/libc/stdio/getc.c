/*	@(#)getc.c	1.2	94/04/07 10:53:39 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * getc.c - read an unsigned character
 */

#include	<stdio.h>

#undef getc	/* but we still can use __getc */

int
getc(stream)
FILE *stream;
{
	return __getc(stream);
}
