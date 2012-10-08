/*	@(#)bullet.c	1.1	96/02/27 14:07:01 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
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
 *	Ed Keizer, 1995 - converted to group bullet server
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
extern	int		bs_sep_ports;
extern	b_fsize		Blksizemin1;


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
 *	All commands are here to get a single threaded UNIX server
 *	working as close to possible like an amoeba bullet server.
 */

/* Return values for command selectors*/
#define	C_REPLIED	0	/* Command has been replied to */
#define C_OK		1	/* Command is set up for reply */
#define C_NOTDONE	2	/* Not in this switch */

static int	super_switch();

/* Commands that return BULLET_LAST_ERR have taken care of the reply
   to the command. Either by themselves or by forwarding the RPC
   to another server
 */

#if defined(USER_LEVEL)
/*ARGSUSED*/
void
bullet_thread(p, s)
char *  p;
int     s;
#else  /* Kernel version */
void
bullet_thread()
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
	bpanic("no request buffer");

    for (;;)
    {
	replybuf = NILBUF;	/* default is null reply */
	/* Get a request */
	hdr.h_port = Superblock.s_fileport;
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

	dbmessage(7,("Got file command %d",hdr.h_command));
	/* Parse the commands */
	switch (hdr.h_command)
	{

	case STD_DESTROY:	/* delete a file */
	    hdr.h_status = bs_std_destroy(&hdr);
	    hdr.h_size = 0;
	    break;

	case STD_INFO:	/* What objects does the server manage */
	    bs_std_info(&hdr,BS_PUBLIC);
	    continue; /* Don't do putrep, gs_std_info did it already */

	case STD_RESTRICT:
	    hdr.h_status = bs_std_restrict(&hdr.h_priv,
		(rights_bits) hdr.h_offset,BS_PUBLIC);
	    hdr.h_size = 0;
	    break;

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

	case BS_INSERT:
	    hdr.h_status = bs_insert(&hdr, reqbuf, (b_fsize) n);
	    hdr.h_size = 0;
	    break;

	case BS_MODIFY:
	    hdr.h_status = bs_modify(&hdr, reqbuf, (b_fsize) n);
	    hdr.h_size = 0;
	    break;

	case BS_READ:
	    bs_read(&hdr, BS_PUBLIC);
	    continue; /* Don't do putrep, bs_read did it already */

	case BS_SIZE:
	    hdr.h_status = bs_size(&hdr);
	    hdr.h_size = 0;
	    break;
	
	default:
	    /* Call super_switch if we respons to super req's */
	    switch( (!bs_sep_ports?super_switch(&hdr,n,reqbuf):C_NOTDONE) )
	    {
	    case C_OK:
	        break ;
	    case C_REPLIED:
	        continue;
	    case C_NOTDONE:
	        Stats.i_lastsweep--;	/* don't count garbage */
	        hdr.h_status = STD_COMBAD;
	        hdr.h_size = 0;
	        break;
	    }
	}
	/* Commands that return BULLET_LAST_ERR have taken care of the
	 * reply
         */
	if ( hdr.h_status!=(command)BULLET_LAST_ERR ) {
#ifdef USE_AM6_RPC
	    rpc_putrep(&hdr, replybuf, hdr.h_size);
#else
	    putrep(&hdr, replybuf, hdr.h_size);
	}
#endif
    }
}

/*
 * SUPER_THREAD
 */

#if defined(USER_LEVEL)
/*ARGSUSED*/
void
super_thread(p, s)
char *  p;
int     s;
#else  /* Kernel version */
void
super_thread()
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
	bpanic("no request buffer");

    for (;;)
    {
	replybuf = NILBUF;	/* default is null reply */
	/* Get a request */
	hdr.h_port = Superblock.s_supercap.cap_port;
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

	dbmessage(7,("Got server command %d",hdr.h_command));
	/* Parse the commands */
	switch( super_switch(&hdr,n,reqbuf) )
	{
	case C_OK:
	    break ;
	case C_REPLIED:
	    continue;
	case C_NOTDONE:
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

static int
super_switch(hdr,n,reqbuf)
header *        hdr;
bufsize		n;		/* #bytes in request buffer */
bufptr		reqbuf;		/* points to request buffer */
{
    /* Parse the commands */
    switch (hdr->h_command)
    {
    case STD_AGE:	/* age files and garbage collect */
        hdr->h_status = bs_std_age(hdr,BS_PUBLIC);
        hdr->h_size = 0;
        break;

    case STD_COPY:	/* replicate a file */
        hdr->h_status = bs_std_copy(hdr, reqbuf, reqbuf + n);
        hdr->h_size = 0;
        break;

    case STD_INFO:	/* What objects does the server manage */
        bs_std_info(hdr,BS_PUBLIC);
        return C_REPLIED ;

    case STD_RESTRICT:
        hdr->h_status = bs_std_restrict(&hdr->h_priv,
	    (rights_bits) hdr->h_offset,BS_PUBLIC);
        hdr->h_size = 0;
        break;

    case STD_STATUS:
#ifdef AMOEBA
	/* User level amoeba */
        gs_status(hdr);
#elif	USER_LEVEL
	/* User level UNIX */
        bs_status(hdr,BS_PUBLIC);
#else
	/* Amoeba kernel */
        gs_status(hdr);
#endif
        return C_REPLIED ;

    case STD_GETPARAMS:
        gs_std_getparams(hdr);
        return C_REPLIED ;

    case STD_SETPARAMS:
        hdr->h_status = gs_std_setparams(hdr, reqbuf, reqbuf + n);
        hdr->h_size = 0;
        break;

    case BS_CREATE:
        hdr->h_status = bs_create(hdr, reqbuf, (b_fsize) n);
        hdr->h_size = 0;
        break;

    case BS_DISK_COMPACT:
        hdr->h_status = gs_disk_compact(hdr);
        hdr->h_size = 0;
        break;

    case BS_FSCK:
        hdr->h_status = gs_fsck(hdr);
        hdr->h_size = 0;
        break;

    case BS_SYNC:
        hdr->h_status = gs_sync(hdr);
        hdr->h_size = 0;
        break;

    case BS_REPCHK:
        hdr->h_status = gs_repchk(hdr);
        hdr->h_size = 0;
        break;

    case BS_ATTACH:
        hdr->h_status = gs_attach(hdr, reqbuf, reqbuf + n, BS_PUBLIC);
        hdr->h_size = 0;
        break;

    case BS_STATE:
        hdr->h_status = gs_state(hdr);
        hdr->h_size = 0;
        break;

    default:
	return C_NOTDONE ;
    }
    /* Commands that return BULLET_LAST_ERR have taken care of the reply */
    if ( hdr->h_status==(command)BULLET_LAST_ERR ) {
	return C_REPLIED ;
    }
    return C_OK;
}
