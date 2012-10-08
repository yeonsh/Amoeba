/*	@(#)calloc.c	1.3	96/02/27 11:11:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "stdlib.h"
#include "string.h"

#ifndef __MAL_DEBUG

_VOIDSTAR	/* "char *" or "void *" depending on compiler */
calloc(num, elmsize)
size_t num;	/* # elements */
size_t elmsize;	/* sizeof each element */
{
    register _VOIDSTAR	p;
    register size_t	size;

    size = num * elmsize;
    if ((p = malloc(size)) != 0)
	memset(p, 0, size);
    return p;
}

#endif
