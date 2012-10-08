/*	@(#)std_age.c	1.3	94/04/07 11:10:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "module/stdcmd.h"

errstat
std_age(cap)
capability *	cap;	/* in: capability for server to be aged */
{
    header	hdr;
    bufsize	status;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = STD_AGE;
    if ((status = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(status);
    return ERR_CONVERT(hdr.h_status);
}
