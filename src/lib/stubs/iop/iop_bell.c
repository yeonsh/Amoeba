/*	@(#)iop_bell.c	1.2	94/04/07 11:02:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "iop/iop.h"

errstat
iop_ringbell(svr, volume, pitch, duration)
capability *	svr;
int		volume;	/* Must be less than 32K */
int		pitch;
int		duration;	/* Must be less than 32K */
{
    header	hdr;
    bufsize	err;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = IOP_BELL;
    hdr.h_extra = volume; /* 16 bits passed! */
    hdr.h_size = duration; /* 16 bits passed! */
    hdr.h_offset = pitch;
    err = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
    if (ERR_STATUS(err))
	return ERR_CONVERT(err);
    return ERR_CONVERT(hdr.h_status);
}
