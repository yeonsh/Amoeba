/*	@(#)iop_unmapmem.c	1.2	94/04/07 11:04:12 */
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
 * Unmap the framebuffer whose memory segment is specified.
 * The user program should have called seg_unmap to get the capability to
 * hand to this routine.
 */

errstat
iop_unmap_mem(svr, video_seg)
capability *	svr;
capability *	video_seg;
{
    header	hdr;
    bufsize	err;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = IOP_UNMAPMEMORY;
    err = trans(&hdr, (bufptr) video_seg, sizeof (capability),
		&hdr, NILBUF, 0);
    if (ERR_STATUS(err))
	return ERR_CONVERT(err);
    return ERR_CONVERT(hdr.h_status);
}
