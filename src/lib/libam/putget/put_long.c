/*	@(#)put_long.c	1.3	94/04/07 10:17:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/buffers.h"

char *
buf_put_long(p, endp, v)
char *	p;
char *	endp;
long 	v;
{
    if (p == 0 || p + sizeof (long) > endp)
	return 0;
    *p++ = v & 0xff;
    *p++ = (v >> 8) & 0xff;
    *p++ = (v >> 16) & 0xff;
    *p++ = (v >> 24) & 0xff;
    return p;
}
