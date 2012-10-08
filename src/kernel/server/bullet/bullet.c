/*	@(#)bullet.c	1.8	96/02/27 14:12:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * BULLET.C
 *
 *	This file contains the Bullet Server Command Parser.
 *
 * Author:
 *	Greg Sharp, Jan 1989
 *	Greg Sharp, Jan 1992 -	added parallelism by adding cache entry
 *				and inode level locking.
 */


#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "module/mutex.h"

#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "stats.h"
#include "cache.h"
#include "bs_protos.h"

extern	Superblk	Superblock;
extern	Statistics	Stats;

/*
 * BULLET_THREAD
 *
 *	This is a full implementation of the bullet server that runs as a
 *	kernel thread, a user program on Amoeba or a user program on UNIX.
 *	It uses a specially allocated buffer as a cache which the disk
 *	driver DMA's directly to and from.  This is allocated to it at boot
 *	time after memory has been sized.  Since it will probably use most
 *	of the main memory, few user threads should have much expectation
 *	of running on the file server.  The percentage of main memory allocated
 *	to the bullet server can be set at kernel compile time.
 *	The bullet server gets its supercap (and thus its server port) from
 *	the super block.  Other capabilities (for disk servers?) are
 *	obtained from the kernel.
 *
 *	The bullet server works with only one virtual disk partition!
 *	The safety factor simply says whether 0 or 1 copies should be made
 *	before replying to the client.  Any replication is handled by the
 *	directory server.
 */

#if defined(USER_LEVEL)
/*ARGSUSED*/
void
bullet_thread(p, s)
char *	p;
int	s;
#else  /* Kernel version */
void
bullet_thread()
#endif
{
    header	hdr;		/* for doing the getrequest */
    bufsize	n;		/* #bytes in request buffer */
    bufptr	reqbuf;		/* points to request buffer */
    bufptr	replybuf;	/* points to reply, if any */

    /* Get a getrequest buffer */
    reqbuf = alloc_buffer((b_fsize) BS_REQBUFSZ);
    if (reqbuf == 0)
	bpanic("no request buffer");

    for (;;)
    {
	replybuf = NILBUF;	/* default is null reply */
	/* Get a request */
	hdr.h_port = Superblock.s_supercap.cap_port;
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, reqbuf, BS_REQBUFSZ);
#else
	n = getreq(&hdr, reqbuf, BS_REQBUFSZ);
#endif
	if (ERR_STATUS(n))
	    bpanic("getreq failed: %d", n);

	/* Record number of requests received since the last sweep */
	Stats.i_lastsweep++;

	/* Parse the commands */
	switch (hdr.h_command)
	{
	case STD_AGE:	/* age files and garbage collect */
	    hdr.h_status = bs_std_age(&hdr);
	    hdr.h_size = 0;
	    break;

	case STD_COPY:	/* replicate a file */
	    hdr.h_status = bs_std_copy(&hdr, reqbuf, reqbuf + n);
	    hdr.h_size = 0;
	    break;

	case STD_DESTROY:	/* delete a file */
	    hdr.h_status = bs_std_destroy(&hdr);
	    hdr.h_size = 0;
	    break;

	case STD_INFO:	/* What objects does the server manage */
	    bs_std_info(&hdr);
	    continue; /* Don't do putrep, bs_std_info did it already */

	case STD_RESTRICT:
	    hdr.h_status = bs_std_restrict(&hdr.h_priv, (rights_bits) hdr.h_offset);
	    hdr.h_size = 0;
	    break;

	case STD_STATUS:
	    bs_status(&hdr);
	    continue; /* Don't do putrep, bs_status did it already */

	case STD_TOUCH:
	    hdr.h_status = bs_std_touch(&hdr.h_priv);
	    hdr.h_size = 0;
	    break;

	case STD_NTOUCH:
	    /* Make sure the array size is correct */
	    if (hdr.h_size * sizeof (private) != n)
	    {
		hdr.h_status = STD_ARGBAD;
		hdr.h_extra = 0;
	    }
	    else
		hdr.h_status = bs_std_ntouch(hdr.h_size, (private *) reqbuf, &hdr.h_extra);
	    hdr.h_size = 0;
	    break;

	case BS_CREATE:
	    hdr.h_status = bs_create(&hdr, reqbuf, (b_fsize) n);
	    hdr.h_size = 0;
	    break;

	case BS_DELETE:
	    hdr.h_status = bs_delete(&hdr, reqbuf, n);
	    hdr.h_size = 0;
	    break;

	case BS_DISK_COMPACT:
	    hdr.h_status = bs_disk_compact(&hdr);
	    hdr.h_size = 0;
	    break;

	case BS_FSCK:
	    hdr.h_status = bs_fsck(&hdr);
	    hdr.h_size = 0;
	    break;

	case BS_INSERT:
	    hdr.h_status = bs_insert(&hdr, reqbuf, (b_fsize) n);
	    hdr.h_size = 0;
	    break;

	case BS_MODIFY:
	    hdr.h_status = bs_modify(&hdr, reqbuf, (b_fsize) n);
	    hdr.h_size = 0;
	    break;

	case BS_READ:
	    bs_read(&hdr);
	    continue; /* Don't do putrep, bs_read did it already */

	case BS_SIZE:
	    hdr.h_status = bs_size(&hdr);
	    hdr.h_size = 0;
	    break;
	
	case BS_SYNC:
	    hdr.h_status = bs_sync(&hdr);
	    hdr.h_size = 0;
	    break;

	default:
	    Stats.i_lastsweep--;	/* don't count garbage */
	    hdr.h_status = STD_COMBAD;
	    hdr.h_size = 0;
	    break;
	}
#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, replybuf, hdr.h_size);
#else
	putrep(&hdr, replybuf, hdr.h_size);
#endif
    }
}
