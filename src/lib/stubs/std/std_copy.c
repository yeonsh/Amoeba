/*	@(#)std_copy.c	1.4	96/02/27 11:18:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "module/stdcmd.h"
#include "module/buffers.h"

errstat
std_copy(server, orig, new)
capability *	server;	/* in: capability of server to make the copy */
capability *	orig;	/* in: capability of object to be copied */
capability *	new;	/* out: capability of newly copied object */
{
    header	hdr;
    bufsize	status;
    char	buf[sizeof(capability)];
    char *	p;

    if ((p = buf_put_cap(buf, buf + sizeof(capability), orig)) == 0)
	return STD_SYSERR;
    hdr.h_port = server->cap_port;
    hdr.h_priv = server->cap_priv;
    hdr.h_command = STD_COPY;
    if ((status = trans(&hdr, buf, p - buf, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(status);
    if (hdr.h_status == STD_OK)
    {
	new->cap_port = hdr.h_port;
	new->cap_priv = hdr.h_priv;
    }
    return ERR_CONVERT(hdr.h_status);
}
