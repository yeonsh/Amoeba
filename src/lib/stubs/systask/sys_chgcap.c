/*	@(#)sys_chgcap.c	1.1	96/02/27 11:19:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This stub is used by the reservation server to get a kernel to modify
 * its kernel directory, sys and proc capabilities.
 *
 * Author: Gregory J. Sharp, May 1994
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "systask/systask.h"
#include "module/buffers.h"


#define	BUFSZ	(3 * sizeof (port))

errstat
sys_chgcap(svr, timeout, chkfields)
capability *	svr;		/* in:  capability for kernel's systask */
interval	timeout;	/* in:  duration of capability change */
port *		chkfields;	/* in:  3 new checkfields */
{
    header	hdr;
    bufsize	t;
    char	buf[BUFSZ];
    char *	p;
    char *	e;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = SYS_CHGCAP;
    hdr.h_offset = timeout;

    e = buf + BUFSZ;
    p = buf_put_port(buf, e, &chkfields[0]);
    p = buf_put_port(p, e, &chkfields[1]);
    p = buf_put_port(p, e, &chkfields[2]);
    if (p == 0)
	return STD_SYSERR;

    t = trans(&hdr, (bufptr) buf, BUFSZ, &hdr, NILBUF, 0);
    if (ERR_STATUS(t))
	return ERR_CONVERT(t);
    return ERR_CONVERT(hdr.h_status);
}
