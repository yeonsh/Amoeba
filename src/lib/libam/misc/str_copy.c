/*	@(#)str_copy.c	1.2	94/04/07 10:08:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <string.h>

char *str_copy(s)
char *s;
{
	char *new, *p;

	if ((new = (char *) malloc((size_t) (strlen(s) + 1))) != 0) {
		p = new;
		while ((*p++ = *s++) != 0)
			;
	}
	return new;
}
