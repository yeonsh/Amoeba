/*	@(#)mc68000.c	1.2	94/04/07 14:29:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "mc68000/machdep.h"

void
mc68000_info(high, shift, size)
long *high, *shift, *size;
{
    *high = VIR_HIGH;
    *shift = CLICKSHIFT;
    *size = CLICKSIZE;
}
