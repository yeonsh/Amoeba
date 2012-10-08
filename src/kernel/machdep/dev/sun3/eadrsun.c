/*	@(#)eadrsun.c	1.2	94/04/06 09:25:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"

#define PROMADDR 0L
#define EAOFFSET 2

etheraddr(eaddr) char *eaddr; {
	register long i;

	for (i=0;i<6;i++)
		eaddr[i] = fctlb(PROMADDR+EAOFFSET+i);
}
