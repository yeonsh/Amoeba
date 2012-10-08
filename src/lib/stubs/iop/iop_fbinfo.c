/*	@(#)iop_fbinfo.c	1.3	94/04/07 11:02:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "iop/iop.h"
#include "module/buffers.h"

/* Returns the framebuffer descriptor in *fbinfo */

errstat
iop_framebufferinfo(svr, fbinfo)
capability *		svr;
IOPFrameBufferInfo *	fbinfo;
{
    header	hdr;
    bufsize	sz;
    char	buf[sizeof (IOPFrameBufferInfo)];
    char *	p;
    char *	e;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = IOP_FRAMEBUFFERINFO;
    sz = trans(&hdr, NILBUF, 0, &hdr, (bufptr) buf,  sizeof (*fbinfo));
    if (ERR_STATUS(sz))
	return ERR_CONVERT(sz);
    if (hdr.h_status != STD_OK)
	return ERR_CONVERT(hdr.h_status);

    /* Else unpack the struct */
    e = buf + sz;
    p = buf_get_long(buf, e, &fbinfo->type);
    p = buf_get_short(p, e, (short *) &fbinfo->width);
    p = buf_get_short(p, e, (short *) &fbinfo->height);
    p = buf_get_short(p, e, (short *) &fbinfo->depth);
    p = buf_get_short(p, e, (short *) &fbinfo->stride);
    p = buf_get_short(p, e, (short *) &fbinfo->xmm);
    p = buf_get_short(p, e, (short *) &fbinfo->ymm);
    p = buf_get_cap(p, e, &fbinfo->fb);
    p = buf_get_bytes(p, e, fbinfo->name, IOPNAMELEN);
    if (p == 0)
	return STD_SYSERR;
    return STD_OK;
}
