/*	@(#)atexit.c	1.2	94/04/07 10:46:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "_ARGS.h"

#define	NEXITS	32

extern void (*__functab[NEXITS]) _ARGS((void));
extern int __funccnt;

int
atexit(func)
void (*func) _ARGS((void));
{
	if (__funccnt >= NEXITS)
		return 1;
	__functab[__funccnt++] = func;
	return 0;
}
