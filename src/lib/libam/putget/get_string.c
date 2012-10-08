/*	@(#)get_string.c	1.3	94/04/07 10:17:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/buffers.h"

char *
buf_get_string(p, e, s)
char *	p;
char *	e;
char **	s;
{
    if ((*s = p) != 0)
	while (p < e)
	    if (*p++ == 0)
		return p;
    return 0;
}
