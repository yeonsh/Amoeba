/*	@(#)get_port.c	1.3	94/04/07 10:16:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** buf_get_port
**	Get a port from a buffer.
**	For ameoba 4.0 use of this will be essential since a capability
**	may contain integers! To be independent of amoeba versions
**	this routine must be used.
*/

#include "amoeba.h"
#include "string.h"
#include "module/buffers.h"

char *
buf_get_port(p, endbuf, prt)
char *	p;
char *	endbuf;
port *	prt;
{
    if (p == 0 || p + sizeof (port) > endbuf)
	return 0;
    (void) memmove((char *) prt, p, sizeof (port));
    return p + sizeof (port);
}
