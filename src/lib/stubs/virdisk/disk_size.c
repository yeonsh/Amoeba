/*	@(#)disk_size.c	1.3	96/02/27 11:21:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** DISKSIZE
**	This is the client stub for the disk_size command.
**	It returns in "size" the number of virtual disk blocks of 2^"l2vblksz"
**	that fit on the virtual disk specified by "cap".
** Author:
**	Greg Sharp 07-02-89
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"

errstat
disk_size(cap, l2vblksz, size)
capability *	cap;		/* in: capability for virtual disk */
int		l2vblksz;	/* in: log2 of size of virtual disk block */
disk_addr *	size;		/* out: # diskblocks on virtual disk */
{
    header	hdr;
    bufsize	n;

/* Can't handle BIG virtual blocks */
    if ((1 << l2vblksz) > D_REQBUFSZ)
	return STD_ARGBAD;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_size = l2vblksz;
    hdr.h_command = DS_SIZE;

    if ((n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != STD_OK)
	return (short) n; /*AMOEBA4*/

    *size = (disk_addr) hdr.h_offset;
    return (short) hdr.h_status; /*AMOEBA4*/
}
