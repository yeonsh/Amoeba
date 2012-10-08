/*	@(#)getchar.c	1.2	94/04/07 10:53:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * getchar.c - read a character from the standard input stream
 */

#include	<stdio.h>

#undef getchar	/* but we still can use __getchar */

int
getchar()
{
	return __getchar();
}
