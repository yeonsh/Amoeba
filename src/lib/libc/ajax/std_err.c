/*	@(#)std_err.c	1.2	94/04/07 10:32:01 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <string.h>

std_err(s)
	char *s;
{
	write(2, s, strlen(s));
}
