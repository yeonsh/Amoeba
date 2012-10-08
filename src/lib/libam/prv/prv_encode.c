/*	@(#)prv_encode.c	1.4	96/02/27 11:03:46 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/crypt.h"
#include "module/prv.h"

int
prv_encode(prv, obj, rights, random)
register private *	prv;
objnum			obj;
rights_bits		rights;
port *			random;
{
/* Amoeba 5 restrictions on size of object number */
    if (obj < 0 || obj >= 0x1000000)
	return -1;
    prv->prv_object[0] = obj;
    prv->prv_object[1] = obj >>  8;
    prv->prv_object[2] = obj >> 16;
    prv->prv_rights = rights & PRV_ALL_RIGHTS;
    if (random != 0)
    {
	prv->prv_random = *random;
	if ((rights & PRV_ALL_RIGHTS) != PRV_ALL_RIGHTS)
	{
	    prv->prv_random._portbytes[0] ^= rights;
	    one_way(&prv->prv_random, &prv->prv_random);
	}
    }
    return 0;
}
