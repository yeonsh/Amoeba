/*	@(#)put_port.c	1.3	94/04/07 10:18:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** buf_put_port
**	Put a port into a buffer.
**	For ameoba 4.0 this will be essential since a capability
**	may contain integers! To be independent of amoeba versions
**	this routine must be used.
*/

#include "amoeba.h"
#include "string.h"
#include "module/buffers.h"

char *
buf_put_port(p, endbuf, prt)
char *	p;
char *	endbuf;
port *	prt;
{
    if (p == 0 || p + sizeof (port) > endbuf)
	return 0;
    (void) memmove(p, (char *) prt, sizeof (port));
    return p + sizeof (port);
}
