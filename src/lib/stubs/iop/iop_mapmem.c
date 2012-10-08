/*	@(#)iop_mapmem.c	1.2	94/04/07 11:03:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "iop/iop.h"

/*
 * Create a Hardware segment for the framebuffer starting at addr, size bytes
 * long.  The capability for the hardware segment is returned to the caller
 * who can later map it into his/her address space using seg_map.
 */

errstat
iop_map_mem(svr, video_seg)
capability *	svr;
capability *	video_seg;
{
    header	hdr;
    bufsize	err;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = IOP_MAPMEMORY;
    err = trans(&hdr, (bufptr) video_seg, sizeof (capability),
		&hdr, (bufptr) video_seg, sizeof (capability));
    if (ERR_STATUS(err))
	return ERR_CONVERT(err);
    if (err != sizeof (capability))
	return STD_SYSERR;
    return ERR_CONVERT(hdr.h_status);
}
