/*	@(#)get_long.c	1.3	94/04/07 10:12:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  buf_get_long
**	Tries to get a long from the buffer 'p'.  the end of buffer 'p' is
**	'endp' and if a long doesn't fit then return 0.
**	otherwise return the next position in the buffer after the long.
**	As a special feature it returns 0 if p is 0 so that a whole bunch
**	of puts or gets can be done and then a single check made at the end
**	to see if any failed.
*/
#include "amoeba.h"
#include "module/buffers.h"

char *
buf_get_long(p, endp, val)
char *	p;
char *	endp;
long *	val;
{
    register long r;

    if (p == 0 || p + sizeof (long) > endp)	/* make sure it fits */
	return 0;
    r = *p++ & 0xFF;
    r |= (long) (*p++ & 0xFF) << 8;
    r |= (long) (*p++ & 0xFF) << 16;
    r |= (long) (*p++ & 0xFF) << 24;
    *val = r;
    return p;
}
