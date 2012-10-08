/*	@(#)put_short.c	1.3	94/04/07 10:18:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/buffers.h"

#ifdef __STDC__
char *
buf_put_short(char * p, char * endp, short v)

#else /* __STDC__ */
char *
buf_put_short(p, endp, v)
char *	p;
char *	endp;
short	v;
#endif /* __STDC__ */
{
    if (p == 0 || p + sizeof (short) > endp)
	return 0;
    *p++ = v & 0xff;
    *p++ = (v >> 8) & 0xff;
    return p;
}
