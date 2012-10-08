/*	@(#)tempnam.c	1.2	94/04/07 10:55:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "loc_incl.h"

#ifndef P_tmpdir
#define P_tmpdir "/tmp/"
#endif

char *
tempnam(dir, pfx)
	char *dir;
	char *pfx;
{
	char *s, *p;
	
	if (dir == NULL)
		dir = getenv("TMPDIR");
	if (dir == NULL || dir[0] == '\0')
		dir = P_tmpdir;
	if (pfx == NULL)
		pfx = "";
	s = (char *)malloc(strlen(dir) + strlen(pfx) + 8 + 1);
	if (s == NULL)
		return NULL;
	strcpy(s, dir);
	p = strchr(s, '\0');
	if (p > s && p[-1] != '/')
		*p++ = '/';
	strcpy(p, pfx);
	p = strchr(p, '\0');
	p = _i_compute(_temp_id(), 16, p, 8);
	*p++ = '\0';
	return s;
}
