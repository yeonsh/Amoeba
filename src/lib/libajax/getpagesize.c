/*	@(#)getpagesize.c	1.2	94/04/07 09:44:32 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getpagesize(2) system call emulation */

#include "ajax.h"

#define PAGESIZE 1024 /* Try to prove that ld works */

int
getpagesize()
{
#ifdef PAGESIZE
	return PAGESIZE;
#else
	capability self;
	int clickshift, first, last;
	
	if (getinfo(&self, NILPD, 0) < 0)
		ERR(EIO, "getpagesize: getinfo failed");
	if (getdef(&self, &clickshift, &first, &last) < 0)
		ERR(EIO, "getpagesize: getdef failed");
	return 1<<clickshift;
#endif
}
