/*	@(#)iop_getev.c	1.3	96/02/27 11:15:14 */
/*
 * Copyright 1994, 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "iop/iop.h"

errstat
iop_getevents(svr, eventq, size)
capability *	svr;
IOPEvent *	eventq;
int *		size; /* in = max #events in eventq, out = # events returned */
{
    header	hdr;
    bufsize	err;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = IOP_GETEVENTS;
    hdr.h_size = *size;
    err = trans(&hdr, NILBUF, 0,
		&hdr, (bufptr) eventq, hdr.h_size * sizeof (IOPEvent));
    if (ERR_STATUS(err))
	return ERR_CONVERT(err);
    if (hdr.h_status == STD_OK)
    {
	/*
	 * We should assert: hdr.h_size <= *size
	 *  since *size is the max # events we can fit in eventq.
	 */
	*size = hdr.h_size;
    }
    return ERR_CONVERT(hdr.h_status);
}
