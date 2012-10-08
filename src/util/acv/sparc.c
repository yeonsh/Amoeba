/*	@(#)sparc.c	1.2	94/04/07 14:30:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sparc/machdep.h"

void
sparc_info(high, shift, size)
long *high, *shift, *size;
{
    *high = VIR_HIGH;
    *shift = CLICKSHIFT;
    *size = CLICKSIZE;
}

    
