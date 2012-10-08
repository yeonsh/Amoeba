/*	@(#)disk_ctl.c	1.3	96/02/27 11:21:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** DISK_CONTROL
**	This is the client stub for the disk_control command.
**
** Author:
**	Greg Sharp 26-04-93
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"


#ifdef __STDC__
errstat
disk_control(capability * cap, int cmd, bufptr buf, bufsize size)
#else
errstat
disk_control(cap, cmd, buf, size)
capability *	cap;		/* in: capability for virtual disk */
int		cmd;
bufptr		buf;
bufsize		size;
#endif
{
    header	hdr;
    bufsize	n;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = DS_CONTROL;
    hdr.h_extra = cmd;

    n = trans(&hdr, buf, size, &hdr, buf, size);
    if (ERR_STATUS(n))
	return ERR_CONVERT(n);
    return ERR_CONVERT(hdr.h_status);
}
