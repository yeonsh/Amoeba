/*	@(#)ether_aton.c	1.3	94/04/07 10:47:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  ETHER_ATON
**
**	This routine parses the array pointed to by "line" which should be in
**	the format %x:%x:%x:%x:%x:%x and converts it to a struct ether_addr.
**	It returns a pointer to the static data.
**	Arguments are assumed sensible.  Null pointers will probably cause
**	exceptions.
**	Author: Gregory J. Sharp, July 1990
*/

#include "ether_addr.h"
#include "stdlib.h"

struct ether_addr *
ether_aton(line)
char *	line;
{
    static struct ether_addr eaddr;
    register int i;
    register unsigned long val;

/* read the ethernet address */
    for (i = 0; i < 5; i++)
    {
	val = (unsigned long) strtol(line, &line, 16);
	if (val > 255 || *line++ != ':')
	    return 0;
	eaddr.ea_addr[i] = val & 0xff;
    }
    val = (unsigned long) strtol(line, &line, 16);
    if (val > 255)
	return 0;
    eaddr.ea_addr[i] = val & 0xff;

    return &eaddr;
}
