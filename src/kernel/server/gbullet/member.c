/*	@(#)member.c	1.1	96/02/27 14:07:30 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * MEMBER.C
 *
 *	This file contains the command parser for the private port
 *	of a storage server.
 *
 * Author:
 *	Ed Keizer, based on bullet.c
 */


#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "module/mutex.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "stats.h"
#include "cache.h"
#include "bs_protos.h"
#include "grp_defs.h"

extern	Superblk	Superblock;
extern	Statistics	Stats;

extern	b_fsize		Blksizemin1;

/* make proper preprocessing symbol for `amoeba-only' */

#ifdef AMOEBA
#define	USE_AMOEBA	1
#else
#ifndef USER_LEVEL
#define	USE_AMOEBA	1
#endif
#endif

/*
 *
 *	This part is not started under the UNIX version
 *	The bullet server works with only one virtual disk partition!
 */

#ifdef USE_AMOEBA
#if defined(USER_LEVEL)
/*ARGSUSED*/
void
member_thread(p, s)
char *  p;
int     s;
#else  /* Kernel version */
void
member_thread()
#endif
{
    header	hdr;		/* for doing the getrequest */
    bufsize	n;		/* #bytes in request buffer */
    bufptr	reqbuf;		/* points to request buffer */
    bufptr	replybuf;	/* points to reply, if any */
    b_fsize	buffer_size;	/* Padded buffer size */

    /* Get a getrequest buffer */
    buffer_size= BS_REQBUFSZ ; CEILING_BLKSZ(buffer_size) ;
    reqbuf = alloc_buffer(buffer_size,SEND_CANWAIT);
    if (reqbuf == 0)
	bpanic("no member request buffer");

    for (;;)
    {
	replybuf = NILBUF;	/* default is null reply */
	/* Get a request */
	hdr.h_port = Superblock.s_membercap.cap_port;
	bs_check_state();
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, reqbuf, BS_REQBUFSZ);
#else
	n = getreq(&hdr, reqbuf, BS_REQBUFSZ);
#endif
	if (ERR_STATUS(n)) {
	    if (ERR_CONVERT(n) == RPC_ABORTED) {
		continue ;
	    }
	    bpanic("getreq failed: %d", n);
	}

	bs_check_state();
	/* Record number of requests received since the last sweep */
	Stats.i_lastsweep++;

	dbmessage(7,("Got member command %d",hdr.h_command));
	/* Parse the commands */
	switch (hdr.h_command)
	{
	case STD_AGE:	/* age files and garbage collect */
	    hdr.h_status = bs_std_age(&hdr,BS_PEER);
	    hdr.h_size = 0;
	    break;

	case STD_INFO:	/* What objects does the server manage */
	    bs_std_info(&hdr,BS_PEER);
	    continue; /* Don't do putrep, bs_std_info did it already */

	case STD_RESTRICT:
	    hdr.h_status = bs_std_restrict(&hdr.h_priv,
		(rights_bits) hdr.h_offset, BS_PEER);
	    hdr.h_size = 0;
	    break;

	case STD_STATUS:
	    bs_status(&hdr,BS_PEER);
	    continue; /* Don't do putrep, bs_status did it already */

	case STD_GETPARAMS:
            bs_std_getparams(&hdr);
            continue ;

	case STD_SETPARAMS:
            hdr.h_status = bs_std_setparams(&hdr, reqbuf, reqbuf + n);
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

	case BS_READ:
	    bs_read(&hdr, BS_PEER);
	    continue; /* Don't do putrep, bs_read did it already */

	case BS_SYNC:
	    hdr.h_status = bs_sync(&hdr);
	    hdr.h_size = 0;
	    break;

	case BS_REPCHK:
	    hdr.h_status = bs_repchk(&hdr);
	    hdr.h_size = 0;
	    break;

	case BS_ATTACH:
	    hdr.h_status = gs_attach(&hdr, reqbuf, reqbuf + n, BS_PEER);
	    hdr.h_size = 0;
	    break;

	case BS_STATE:
            hdr.h_status = bs_state(&hdr);
	    hdr.h_size = 0;
	    break;

	case BS_WELCOME:
	    gs_welcome(&hdr, reqbuf, reqbuf + n);
	    continue; /* Don't do putrep, gs_welcome did it already */
	    break;

	case BS_REPLICATE:
	    hdr.h_status = gs_replicate(&hdr, reqbuf, (b_fsize) n);
	    hdr.h_size = 0;
	    break;

	case BS_UPDATE:
	    bs_update(&hdr) ;
	    continue ;
	    break ;

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
#endif
