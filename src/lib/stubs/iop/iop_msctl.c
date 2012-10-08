/*	@(#)iop_msctl.c	1.3	96/02/27 11:15:22 */
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
iop_mousecontrol(svr, type, baud, samplerate, chord_middle)
capability *	svr;
int		type;	/* Must be less than 16K */
int		baud;
int		samplerate;	/* Must be less than 32K */
int		chord_middle;
{
    header	hdr;
    bufsize	err;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = IOP_MOUSECONTROL;
    hdr.h_extra = type;
    if (chord_middle)
	hdr.h_extra |= IOP_CHORDMIDDLE;
    hdr.h_size = samplerate;
    hdr.h_offset = baud;
    err = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
    if (ERR_STATUS(err))
	return ERR_CONVERT(err);
    return ERR_CONVERT(hdr.h_status);
}
