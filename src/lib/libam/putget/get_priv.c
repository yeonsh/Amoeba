/*	@(#)get_priv.c	1.3	94/04/07 10:16:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** buf_get_priv
**	Get a private part of a capability from a buffer.
**	for ameoba 4.0 use of this will be essential since a capability
**	may contain integers! To be independent of amoeba versions
**	this routine must be used.
*/

#include "amoeba.h"
#include "string.h"
#include "module/buffers.h"

char *
buf_get_priv(p, endbuf, priv)
char *		p;
char *		endbuf;
private *	priv;
{
    if (p == 0 || p + sizeof (private) > endbuf)
	return 0;
    (void) memmove((char *) priv, p, sizeof (private));
    return p + sizeof (private);
}
