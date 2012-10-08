/*	@(#)prv_number.c	1.3	96/02/27 11:03:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/prv.h"

objnum
prv_number(prv)
register private *	prv;
{
	
    return  (((objnum) prv->prv_object[0] & 0xFF)     ) |
	    (((objnum) prv->prv_object[1] & 0xFF) << 8) |
	    (((objnum) prv->prv_object[2] & 0xFF) <<16);
}
