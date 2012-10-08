/*	@(#)aalloc.c	1.1	92/06/25 14:48:35 */
/*
 * aalloc.c
 *
 * A simple memory aligned allocation routine.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "param.h"
#include "types.h"

static char *top = (char *)RAMSIZE;
static char last;

char *
aalloc(size, align)
    int size, align;
{
    register char *p;
    register int mask;

    if (align == 0)
	align = sizeof(int);
    mask = align - 1;
    assert((align & mask) == 0);
    top = top - (size + align);
    p = (char *)((int)top & ~mask);
    assert(p > &last);
    assert(p <= (char *) RAMSIZE);
    return top = p;
}
