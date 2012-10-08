/*	@(#)get_short.c	1.3	94/04/07 10:17:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  buf_get_short
**	Tries to get a short from the buffer 'p'.  The end of buffer 'p' is
**	'endp' and if a short doesn't fit then return 0.
**	Otherwise return the next position in the buffer after the short.
**	As a special feature it returns 0 if p is 0 so that a whole bunch
**	of puts or gets can be done and then a single check made at the end
**	to see if any failed.
*/
#include "amoeba.h"
#include "module/buffers.h"

char *
buf_get_short(p, endp, val)
char *	p;
char *	endp;
short *	val;
{
    register short r;

    if (p == 0 || p + sizeof (short) > endp)	/* make sure it fits */
	return 0;
    r = *p++ & 0xFF;
    r |= (short) (*p++ & 0xFF) << 8;
    *val = r;
    return p;
}
