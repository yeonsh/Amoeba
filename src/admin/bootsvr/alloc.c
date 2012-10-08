/*	@(#)alloc.c	1.2	94/04/06 11:37:30 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <string.h>
#include "amoeba.h"
#include "monitor.h"

extern char parsstate[];
void prf();

char *
mymalloc(n)
    int n;
{
    char *p;

    if (n <= 0) {
	prf("%nMallocing %d!\n", n);
	abort();
    }
    p = (char *) malloc((size_t) n);
    if (p == NULL) {
	prf("%nmalloc(%d) failure\n", n);
	(void) strcpy(parsstate, "malloc failed");
	MON_EVENT("Malloc failure");
    }
    return p;
} /* mymalloc */
