/*	@(#)flip_oneway.c	1.3	96/02/27 11:01:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "protocols/flip.h"
#include "module/des.h"
#include "module/fliprand.h"

static C_Block nullblock;
static Key_schedule schedule;

void
flip_oneway(prv_addr, pub_addr)
adr_p prv_addr, pub_addr;
{
    adr_t tmp;

    tmp = *prv_addr;
    tmp.a_space = 0;
    if(ADR_NULL(&tmp)) {
	*pub_addr = *prv_addr;
	return;
    }
    des_set_key((C_Block *) prv_addr, &schedule);
    des_ecb_encrypt(&nullblock, (C_Block *) pub_addr, &schedule, DES_ENCRYPT);
    pub_addr->a_space = 0;
    if(ADR_NULL(pub_addr)) {
	des_set_key((C_Block *) pub_addr, &schedule);
	des_ecb_encrypt(&nullblock, (C_Block *) pub_addr, &schedule,
			DES_ENCRYPT);
    }
    pub_addr->a_space = prv_addr->a_space;
}
