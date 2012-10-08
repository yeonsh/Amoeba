/*	@(#)ar_cap.c	1.6	96/02/27 11:00:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**	Print the ascii representation of a capability
*/
#include <stdio.h>
#include "amoeba.h"
#include "module/prv.h"
#include "module/strmisc.h"
#include "module/ar.h"

static char pbuf[15 + 2*8 + 2*2*PORTSIZE];

char *
ar_cap(cap)
capability *	cap;
{
    (void) bprintf(pbuf, pbuf+sizeof(pbuf),
		"%X:%X:%X:%X:%X:%X/%ld(%X)/%X:%X:%X:%X:%X:%X",
		cap->cap_port._portbytes[0] & 0xFF,
		cap->cap_port._portbytes[1] & 0xFF,
		cap->cap_port._portbytes[2] & 0xFF,
		cap->cap_port._portbytes[3] & 0xFF,
		cap->cap_port._portbytes[4] & 0xFF,
		cap->cap_port._portbytes[5] & 0xFF,
		prv_number(&cap->cap_priv),
		cap->cap_priv.prv_rights & 0xFF,
		cap->cap_priv.prv_random._portbytes[0] & 0xFF,
		cap->cap_priv.prv_random._portbytes[1] & 0xFF,
		cap->cap_priv.prv_random._portbytes[2] & 0xFF,
		cap->cap_priv.prv_random._portbytes[3] & 0xFF,
		cap->cap_priv.prv_random._portbytes[4] & 0xFF,
		cap->cap_priv.prv_random._portbytes[5] & 0xFF);
    return pbuf;
}
