/*	@(#)fscontrol.c	1.3	94/04/07 11:02:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "file.h"

errstat
fscontrol(cap, cmd, buf, size)
capability *cap;
uint16 cmd;
char *buf;
uint16 size;
{
    header	hdr;
    bufsize	t;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_size = size;
    hdr.h_extra = cmd;
    hdr.h_command = FSQ_CONTROL;
    if ((t = trans(&hdr, buf, size, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(t);
    return ERR_CONVERT(hdr.h_status);
}
