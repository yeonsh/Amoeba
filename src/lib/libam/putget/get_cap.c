/*	@(#)get_cap.c	1.3	94/04/07 10:12:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	buf_get_cap
**		get a capability from a buffer.
**		for ameoba 4.0 this will be essential since a capability
**		may contain integers! to be independent of amoeba versions
**		this routine must be used.
*/

#include "amoeba.h"
#include "string.h"
#include "module/buffers.h"

char *
buf_get_cap(p, endbuf, cap)
char *		p;
char *		endbuf;
capability *	cap;
{
    if (p == 0 || p + sizeof (capability) > endbuf)
	return 0;
    (void) memmove((char *) cap, p, sizeof (capability));
    return p + sizeof (capability);
}
