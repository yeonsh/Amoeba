/*	@(#)put_cap.c	1.3	94/04/07 10:17:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** buf_put_cap
**	Put a capability into a buffer.
**	For amoeba 4.0 this will be essential since a capability
**	may contain integers! To be independent of amoeba versions
**	this routine must be used.
*/

#include "amoeba.h"
#include "string.h"
#include "module/buffers.h"

char *
buf_put_cap(p, endbuf, cap)
char *		p;
char *		endbuf;
capability *	cap;
{
    if (p == 0 || p + sizeof (capability) > endbuf)
	return 0;
    (void) memmove(p, (char *) cap, sizeof (capability));
    return p + sizeof (capability);
}
