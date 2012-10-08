/*	@(#)tmpfile.c	1.2	94/04/07 10:55:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * tmpfile.c - create and open a temporary file
 */

#include <stdio.h>
#include <string.h>
#include "loc_incl.h"

FILE *
tmpfile()
{
	static char name_buffer[L_tmpnam] = "/tmp/t" ;
	static char *name = NULL;
	FILE *file;

	if (!name) {
		name = name_buffer + strlen(name_buffer);
		name = _i_compute(_temp_id(), 16, name, 8);
		*name = '\0';
	}

	file = fopen(name_buffer,"wb+");
	if (!file) return (FILE *)NULL;
	(void) remove(name_buffer);
	return file;
}
