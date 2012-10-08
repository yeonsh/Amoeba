/*	@(#)down.c	1.3	94/04/06 11:38:24 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "monitor.h"
#include "stderr.h"
#include "cmdreg.h"
#include "svr.h"
#include "module/proc.h"

bool
down(boot)
    obj_rep *boot;
{
    errstat err;

    prf("%nKill %s\n", boot->or.or_data.name);
    if (badport(&boot->or.or_proccap)) err = RPC_NOTFOUND;
    else err = pro_stun(&boot->or.or_proccap, -9L);
    if (err == STD_OK || err == STD_CAPBAD) {
	MON_EVENT("pro_stun returns STD_OK or STD_CAPBAD - great");
	err = STD_OK;
    } else MON_EVENT("Stun: failed");
    return err == STD_OK ? 1 : 0;
} /* down */
