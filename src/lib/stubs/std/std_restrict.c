/*	@(#)std_restrict.c	1.6	96/02/27 11:18:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "module/stdcmd.h"

errstat
std_restrict(cap, mask, new)
capability *	cap;	/* in: capability to be restricted */
rights_bits	mask;	/* in: rights that are to be kept in 'new' */
capability *	new;	/* out: restricted capability */
{
    header	hdr;
    bufsize	status;

    if (cap->cap_priv.prv_rights == PRV_ALL_RIGHTS)
    {
	objnum on;

	on = prv_number(&cap->cap_priv);
	new->cap_port = cap->cap_port;
	if (prv_encode(&new->cap_priv, on, mask, &cap->cap_priv.prv_random) < 0)
	    return STD_CAPBAD;
	return STD_OK;
    }
    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = STD_RESTRICT;
    hdr.h_offset = mask;
    if ((status = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(status);
    if (hdr.h_status == STD_OK)
    {
	new->cap_port = hdr.h_port;
	new->cap_priv = hdr.h_priv;
    }
    return ERR_CONVERT(hdr.h_status);
}
