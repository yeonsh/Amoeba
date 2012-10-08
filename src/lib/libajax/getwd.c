/*	@(#)getwd.c	1.2	94/04/07 09:47:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* BSD-compatible getwd() function */

#include "ajax.h"

extern char *getcwd();

char *
getwd(buf)
	char *buf;
{
	char *p = getcwd(buf, 1025);
	
	if (p == NULL) {
		switch (errno) {
		case ERANGE: strcpy(buf, "path too long"); break;
		case EACCES: strcpy(buf, "can't access path"); break;
		default: strcpy(buf, "sorry, see errno"); break;
		}
	}
	return p;
}
