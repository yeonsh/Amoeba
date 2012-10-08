/*	@(#)ar_port.c	1.6	96/02/27 11:00:39 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**	Print the ascii representation of a port
*/
#include <stdio.h>
#include "amoeba.h"
#include "module/strmisc.h"
#include "module/ar.h"

static char pbuf[PORTSIZE*2 + 5 + 1];

char *
ar_port(p)
port *	p;
{
    (void) bprintf(pbuf, pbuf+sizeof(pbuf), "%X:%X:%X:%X:%X:%X",
			p->_portbytes[0] & 0xFF,
			p->_portbytes[1] & 0xFF,
			p->_portbytes[2] & 0xFF,
			p->_portbytes[3] & 0xFF,
			p->_portbytes[4] & 0xFF,
			p->_portbytes[5] & 0xFF);
    return pbuf;
}
