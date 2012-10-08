/*	@(#)disk_read.c	1.3	96/02/27 11:21:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** DISKREAD
**	This is the client stub for the disk_read command.
**	Since reads may be bigger than fit in a single transaction
**	we loop doing transactions until we are finished.
**	If it can read exactly what was requested it succeeds.
**	Otherwise it fails.  No partial reads are done.
** Author:
**	Greg Sharp 07-02-89
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

errstat
disk_read(cap, l2vblksz, start, numblocks, buf)
capability *	cap;		/* in: capability for virtual disk */
int		l2vblksz;	/* in: log2 of virtual block size */
disk_addr	start;		/* in: # of first block to read */
disk_addr	numblocks;	/* in: # blocks to read */
bufptr		buf;		/* out: buffer to read data into */
{
    header	hdr;
    bufsize	n;
    disk_addr	nblks;		/* # blocks in this transaction */
    disk_addr	maxblocks;	/* max # blocks that fit in trans buffer */
    bufsize	tr_sz;		/* size of transaction */

/* Can't handle BIG virtual blocks */
    if ((1 << l2vblksz) > D_REQBUFSZ || numblocks <= 0 || buf == 0)
	return STD_ARGBAD;

    maxblocks = D_REQBUFSZ >> l2vblksz;

/* the disk task dosen't change the following on a read */
    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;

/* so let's read */
    hdr.h_offset = start;
    do
    {
	hdr.h_command = DS_READ;
	hdr.h_size = l2vblksz;
	nblks = MIN(maxblocks, numblocks);
	hdr.h_extra = nblks;
	tr_sz = nblks << l2vblksz;
	n = trans(&hdr, NILBUF, 0, &hdr, buf, tr_sz);
	if (ERR_STATUS(n))
	    return (short) n; /*AMOEBA4*/
	if (ERR_STATUS(hdr.h_status))
	    break;
	hdr.h_offset += nblks;
	buf += tr_sz;
    } while ((numblocks -= nblks) > 0);
    return (short) hdr.h_status; /*AMOEBA4*/
}
