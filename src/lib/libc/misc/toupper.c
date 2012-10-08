/*	@(#)toupper.c	1.2	94/04/07 10:50:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	ANSI C conforming toupper function
*/

#include "ctype.h"
#undef toupper

int
toupper(c)
int c;
{
	return islower(c) ? c - 'a' + 'A' : c;
}
