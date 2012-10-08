/*	@(#)tmpnam.c	1.2	94/04/07 10:56:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * tmpnam.c - create a unique filename
 */

#include <stdio.h>
#include <string.h>
#include "loc_incl.h"

char *
tmpnam(s)
char *s;
{
	static char name_buffer[L_tmpnam] = "/tmp/t";
	static unsigned long count = 0;
	static char *name = NULL;

	if (!name) { 
		name = name_buffer + strlen(name_buffer);
		name = _i_compute(_temp_id(), 16, name, 8);
		*name++ = '.';
		*name = '\0';
	}
	if (++count > TMP_MAX) count = 1;	/* wrap-around */
	*_i_compute(count, 10, name, 3) = '\0';
	if (s) return strcpy(s, name_buffer);
	else return name_buffer;
}
