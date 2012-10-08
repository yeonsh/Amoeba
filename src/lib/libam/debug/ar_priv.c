/*	@(#)ar_priv.c	1.6	96/02/27 11:00:45 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**	Print the ascii representation of the private part of a capability
*/
#include <stdio.h>
#include "amoeba.h"
#include "module/prv.h"
#include "module/strmisc.h"
#include "module/ar.h"

static char pbuf[8 + 1 + 2*8 + PORTSIZE*2];

char *
ar_priv(p)
private *	p;
{
    (void) bprintf(pbuf, pbuf + sizeof(pbuf), "%ld(%X)/%X:%X:%X:%X:%X:%X",
			prv_number(p),
			p->prv_rights & 0xFF,
			p->prv_random._portbytes[0] & 0xFF,
			p->prv_random._portbytes[1] & 0xFF,
			p->prv_random._portbytes[2] & 0xFF,
			p->prv_random._portbytes[3] & 0xFF,
			p->prv_random._portbytes[4] & 0xFF,
			p->prv_random._portbytes[5] & 0xFF);
    return pbuf;
}
