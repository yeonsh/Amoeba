/*	@(#)ether_ntoa.c	1.3	94/04/07 10:48:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  ETHER_NTOA
**	Convert a binary ethernet address to an ascii string.
**	The argument had better be valid or an exception will probably result.
*/

#include "ether_addr.h"

char *
ether_ntoa(eaddr)
struct ether_addr *	eaddr;
{
    static char	buf[18];

    (void) sprintf(buf, "%x:%x:%x:%x:%x:%x",
			eaddr->ea_addr[0] & 0xff,
			eaddr->ea_addr[1] & 0xff,
			eaddr->ea_addr[2] & 0xff,
			eaddr->ea_addr[3] & 0xff,
			eaddr->ea_addr[4] & 0xff,
			eaddr->ea_addr[5] & 0xff);
    return buf;
}
