/*	@(#)prv_decode.c	1.3	96/02/27 11:03:40 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/prv.h"
#include "module/crypt.h"

int
prv_decode(prv, prights, random)
register private *	prv;
register rights_bits *	prights;
register port *		random;
{
    port tmp;
    
    if ((*prights = prv->prv_rights & PRV_ALL_RIGHTS) == PRV_ALL_RIGHTS)
	return PORTCMP(&prv->prv_random, random) ? 0 : -1;
    tmp = *random;
    tmp._portbytes[0] ^= prv->prv_rights;
    one_way(&tmp, &tmp);
    return PORTCMP(&prv->prv_random, &tmp) ? 0 : -1;
}
