/*	@(#)sys_boot.c	1.4	96/02/27 11:18:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "systask/systask.h"
#include "module/systask.h"
#include "module/buffers.h"

#define	MAXCOMMANDLINE	(128)	/* size of kernel command line */

errstat
sys_boot(cap, kernelcap, commandline, flags)
capability *	cap;		/* in:  capability for kernel's systask */
capability *	kernelcap;	/* in:  capability for new kernel binary */
char *		commandline;	/* in:  kernel command line */
int         	flags;		/* in:  bootflags */
{
    header	hdr;
    bufsize	t;
    char	buf[sizeof(capability) + MAXCOMMANDLINE];
    char *	p;

    if ((p = buf_put_cap(buf, buf + sizeof(buf), kernelcap)) == 0)
	return STD_SYSERR;
    if ((p = buf_put_string(p, buf + sizeof(buf), commandline)) == 0)
	return STD_SYSERR;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = SYS_BOOT;
    hdr.h_extra = flags;

    t = trans(&hdr, buf, p - buf, &hdr, NILBUF, 0);
    if (ERR_STATUS(t))
	return ERR_CONVERT(t);

    return ERR_CONVERT(hdr.h_status);
}
