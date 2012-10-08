/*	@(#)ether_line.c	1.3	94/04/07 10:47:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  ETHER_LINE
**
**	This routine parses the array pointed to by "line" (which should be
**	from a file in the format of /etc/ethers) and returns in "eaddr" the
**	ethernet address at the start of the line and the corresponding host
**	name in "hostname".  It assumes either tabs or spaces separate the
**	two.  The buffer pointed to by "hostname" must be big enough to hold
**	the host name plus a NULL byte.
**	The function returns 0 on success and 1 on failure.
**	Arguments are assumed sensible.  Null pointers will probably cause
**	exceptions.
**	Author: Gregory J. Sharp, July 1990
*/

#include "ctype.h"
#include "ether_addr.h"
#include "stdlib.h"

int
ether_line(line, eaddr, hostname)
char *			line;
struct ether_addr *	eaddr;
char *			hostname;
{
    register int i;
    register unsigned long val;

/* skip leading white space */
    while (*line != '\n' && (*line == ' ' || *line == '\t'))
	line++;

/* read the ethernet address */
    for (i = 0; i < 5; i++)
    {
	val = (unsigned long) strtol(line, &line, 16);
	if (val > 255 || *line++ != ':')
	    return 1;
	eaddr->ea_addr[i] = val & 0xff;
    }
    val = (unsigned long) strtol(line, &line, 16);
    if (val > 255 || (*line != ' ' && *line != '\t'))
	return 1;
    eaddr->ea_addr[i] = val & 0xff;

/* skip leading white space */
    while (*line != '\n' && (*line == ' ' || *line == '\t'))
	line++;

/* read in the hostname */
    while (!isspace(*line))
	*hostname++ = *line++;
    *hostname = '\0';
    return 0;
}
