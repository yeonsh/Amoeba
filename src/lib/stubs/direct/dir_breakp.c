/*	@(#)dir_breakp.c	1.3	94/04/07 11:00:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Break a path up in a directory and a trailing name segment.
   Return NULL if the path leading up to the final name is invalid. */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "direct/direct.h"
#include "module/direct.h"
#include "string.h"
#define _POSIX_SOURCE
#include <posix/limits.h>

#ifndef NULL
#define NULL 0
#endif

char *
dir_breakpath(dir, path, pcap)
capability *dir;
char *path;
capability *pcap;
{
	char *p;
	int n;
	char pathbuf[PATH_MAX+1];
	
	if (path == NULL)
		return NULL; /* No path */
	p = strrchr(path, '/');
	if (p == NULL) {
		if (!dir)
			/* Interpret path relative to default origin: */
			*pcap = *dir_origin(path);
		else
			*pcap = *dir;
		return path;
	}
	n = p - path;
	if (n >= sizeof pathbuf)
		return NULL; /* Path too long */
	strncpy(pathbuf, path, n);
	pathbuf[n] = '\0';
	if (dir_lookup(dir, pathbuf, pcap) != STD_OK)
		return NULL; /* Path leading up to the tail invalid */
	return p+1;
}
