/*	@(#)disk_geom.c	1.3	96/02/27 11:21:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** DISK_GETGEOMETRY
**	This is the client stub for the disk_getgeometry command.
**	It returns in "geom" the geometry information for the first
**	"piece" of a virtual disk.  It is only intended for bootp disks
**	but will tell about other disks as well.
**
** Author:
**	Greg Sharp 11-06-91
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"
#include "module/buffers.h"


errstat
disk_getgeometry(cap, geom)
capability *	cap;		/* in: capability for virtual disk */
geometry *	geom;		/* out: geometry of virtual disk */
{
    header	hdr;
    bufsize	n;
    char *	p;
    char *	e;
    char	buf[sizeof (geometry)];

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = DS_GETGEOMETRY;

    n = trans(&hdr, NILBUF, 0, &hdr, (bufptr) buf, sizeof (geometry));
    if (ERR_STATUS(n))
	return ERR_CONVERT(n);
    if (ERR_STATUS(hdr.h_status))
	return ERR_CONVERT(hdr.h_status);

    e = buf + n;
    p = buf_get_int16(buf, e, &geom->g_bpt);
    p = buf_get_int16(p, e, &geom->g_bps);
    p = buf_get_int16(p, e, &geom->g_numcyl);
    p = buf_get_int16(p, e, &geom->g_altcyl);
    p = buf_get_int16(p, e, &geom->g_numhead);
    p = buf_get_int16(p, e, &geom->g_numsect);
    if (p == 0)
	return STD_SYSERR;
    return STD_OK;
}
