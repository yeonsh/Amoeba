/*	@(#)tolower.c	1.2	94/04/07 10:50:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	ANSI C conforming tolower function
*/

#include "ctype.h"
#undef tolower

int
tolower(c)
int c;
{
	return isupper(c) ? c - 'A' + 'a' : c;
}
