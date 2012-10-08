/*	@(#)x11_shutdown.c	1.3	94/04/07 11:16:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <server/x11/Xamoeba.h>

errstat
x11_shutdown(cap)
    capability *cap;
{
    header hdr;
    bufsize rv;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = AX_SHUTDOWN;
    rv = trans(&hdr, 0, 0, &hdr, 0, 0);
    if (ERR_STATUS(rv))
	return ERR_CONVERT(rv);
    return ERR_CONVERT(hdr.h_status);
}
