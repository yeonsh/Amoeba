/*	@(#)utimes.c	1.2	94/04/07 09:55:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* BSD utimes(2) emulation */

#include "ajax.h"
#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>

int
utimes(path, tvp)
	char *path;
	struct timeval tvp[2];
{
	struct utimbuf times;
	
	times.actime = tvp[0].tv_sec;
	times.modtime = tvp[1].tv_sec;
	return utime(path, &times);
}
