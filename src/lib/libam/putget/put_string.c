/*	@(#)put_string.c	1.4	94/04/07 10:18:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/buffers.h"

#ifndef __STDC__
#define const /**/
#endif

char *
buf_put_string(p, e, s)
char       * p;
char       * e;
const char * s;
{
    if (p != 0)
	while (p < e)
	    if ((*p++ = *s++) == 0)
		return p;
    return 0;
}
