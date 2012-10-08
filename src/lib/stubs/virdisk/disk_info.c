/*	@(#)disk_info.c	1.4	96/02/27 11:21:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** DISK_INFO
**	This is the client stub for the disk_info command.
**	It returns in "buf" an array of disk_addr's which are pairs of
**	(firstblock, # blocks).
** Author:
**	Greg Sharp 17-05-90
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"
#include "module/buffers.h"

char *	buf_get_disk_addr();

errstat
disk_info(cap, buf, size, cnt)
capability *	cap;		/* in: capability for virtual disk */
dk_info_data *	buf;		/* out: details of virtual disk */
int		size;		/* in: size of buf in "disk_addr pairs" */
int *		cnt;		/* out: number of pairs returned */
{
    header	hdr;
    bufsize	n;
    int 	i;
    char *	p;
    char *	e;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = DS_INFO;

    size *= sizeof (dk_info_data);  /* convert size to bytes */
    n = trans(&hdr, NILBUF, 0, &hdr, (bufptr) buf, size);
    if (ERR_STATUS(n))
	return ERR_CONVERT(n);
    if (ERR_STATUS(hdr.h_status))
	return ERR_CONVERT(hdr.h_status);

/* if we get back more than we asked for we just return what they asked for */
    if (hdr.h_size > (unsigned) size)
	hdr.h_size = size;

/* unpack the data into the struct - assume we can do it in place for now */
    *cnt = hdr.h_size / DISKINFOSZ;
    p = (char *) buf;
    e = p + size;
    for (i = 0; i < *cnt; i++)
    {
	p = buf_get_int32(p, e, &buf->d_unit);
	p = buf_get_disk_addr(p, e, &buf->d_firstblk);
	p = buf_get_disk_addr(p, e, &buf->d_numblks);
    }
    if (p == 0)
	return STD_SYSERR;
    return STD_OK;
}
