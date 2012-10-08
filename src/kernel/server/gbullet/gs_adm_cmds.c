/*	@(#)gs_adm_cmds.c	1.1	96/02/27 14:07:24 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * GS_ADM_CMDS.C
 *
 *	This file contains the implementation of the bullet server
 *	administrative commands.
 *
 * Author:
 *	Greg Sharp, Jan 1989
 * Modified:
 *	Greg Sharp, Jan 1992 - Changed locking granularity to be much finer.
 *	Ed Keizer, 1995	     - To fit in with group bullet
 */

#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/prv.h"
#include "module/mutex.h"
#include "module/strmisc.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "alloc.h"
#include "stats.h"
#include "cache.h"
#include "bs_protos.h"
#include "preempt.h"
#include "assert.h"
#include "grp_defs.h"
#include "grp_comm.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif

extern	Superblk	Superblock;	/* in-core copy of superblock */
extern	Statistics	Stats;
extern	Inode *		Inode_table;	/* pointer to in-core inode table */
extern	bufptr		Bc_begin;	/* Start address of buffer cache */
extern	bufptr		Bc_end;		/* End address of buffer cache */
extern	b_fsize		Blksizemin1;

/*
 * GS_STATUS
 *
 *	This function fills the given buffer with status
 *	information in the form of an ascii string.
 *	The report should look vaguely like this:
 *
 *	......
 *
 *	The status information should be less than 1K so we put it on the
 *	stack.  This routine must do a putrep before returning.
 */

void
gs_status(hdr)
header *	hdr;	/* request header for this command */
{
    bufptr	buf;
    char *	end;
    errstat	err;
    char	Buf[BS_STATUS_SIZE];

    Stats.g_status++;

    /* Make sure that they have the super capability for this server */
    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_READ,BS_PUBLIC)) != STD_OK)
    {
        hdr->h_status = err;
	hdr->h_size = 0;
    }
    else
    {
	buf = Buf;
	end = Buf + BS_STATUS_SIZE;

	buf= grp_status(buf, end) ;

	hdr->h_status = STD_OK;
	hdr->h_size = buf - Buf;
    }
#ifdef USE_AM6_RPC
    rpc_putrep(hdr, Buf, hdr->h_size);
#else
    putrep(hdr, Buf, hdr->h_size);
#endif /* USE_AM6_RPC */
    return;
}

/* A stub used by gs_sync, and possibly others
 * to check for a legal call and pass the hdr to all members.
 */

static errstat
pass_through(hdr,func)
header *	hdr;
errstat		(*func)() ;
{
    errstat	err;
    errstat	last_err;

    /* Make sure that they have the super capability for this server */
    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_READ,BS_PUBLIC)) == STD_OK)
    {
	err= bs_grp_all(hdr, (bufptr) 0, 0);
	/* Call your own last .. */
	last_err= (*func)();
	if ( last_err!=STD_OK ) err= last_err ;
    }
    return err;
}


/*
 * GS_SYNC, GS_DISK_COMPACT and GS_FSCK are passed to all active members.
 *
 */

errstat
gs_sync(hdr)
header *	hdr;
{
	Stats.g_sync++;
	return pass_through(hdr,bs_do_sync) ;
}

errstat
gs_disk_compact(hdr)
header *	hdr;
{
	Stats.g_disk_comp++;
	return pass_through(hdr,bs_do_compact) ;
}

errstat
gs_fsck(hdr)
header *	hdr;
{
	Stats.g_fsck++;
	return pass_through(hdr,bs_do_fsck) ;
}

errstat
gs_repchk(hdr)
header *	hdr;
{
	Stats.g_fsck++;
	return pass_through(hdr,bs_do_repchk) ;
}


/*
 *	A member is changing state through public port.
 *
 */

errstat
gs_state(hdr)
header *	hdr;
{
    errstat	err ;
    int		action ;
    int		member ;

    Stats.g_state++;
    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PUBLIC)) != STD_OK) {
	return err;
    }

    action= hdr->h_extra & BS_STATE_MASK ;

    /* If the force flag is off, we take the freedom to check now 
     * whether everybody is there.
     */
    if ( action!=BS_EXIT &&
	 !(hdr->h_extra&BS_STATE_FORCE) && bs_grp_missing()!=0 ) {
	return STD_NOTNOW ;
    }

    member= hdr->h_offset ;
    if ( member==Superblock.s_member )  {
	return bs_do_state(hdr,BS_PUBLIC) ;
    }

    if ( member>=0 ) {
	switch ( action ) {
	case BS_TESTSTABLE:
	case BS_EXIT:
	    /* Defer to the member in question */
	    return bs_grp_single(member,hdr,(bufptr)0,0);
	case BS_PASSIVE:
	case BS_DETACH:
	    return bs_do_state(hdr,BS_PUBLIC) ;
	default:
	    return STD_ARGBAD;
	}
    }
    /* Only here if member is negative */
    if ( member!= -1 ) return STD_ARGBAD ;
    switch ( action ) {
    case BS_TESTSTABLE:
    case BS_EXIT:
	err= bs_grp_all(hdr, (bufptr) 0, 0);
	if ( err!=STD_OK ) return ;
	return bs_do_state(hdr,BS_PUBLIC) ;
    default:
	/* We do not allow state switching for all members at once */
	return STD_COMBAD ;
    }
    /* NOTREACHED */
}
