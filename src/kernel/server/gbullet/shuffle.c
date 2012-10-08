/*	@(#)shuffle.c	1.1	96/02/27 14:07:37 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * SHUFFLE.C
 *
 * This package manages the shuffling of inode resources between
 * members of the group. It uses only members currently active.
 * The full group need not be present for reshuffling.
 * Multiple members may initiate reshuffling at the same time.
 * A member looks the member with the least number of inodes.
 * It then shares it's inodes with hat other member.
 * Member will not part with inodes when it causes them to sink below
 * what they think the have a right to (the mean).
 *
 * Author:
 *	Ed Keizer,  June 1993
 */

#include "amoeba.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */
#include "byteorder.h"
#include "module/rnd.h"
#include "module/mutex.h"
#include "string.h"

#include "module/disk.h"
#include "group.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "semaphore.h"
#include "alloc.h"
#include "cache.h"
#include "preempt.h"
#include "bs_protos.h"
#include "grp_bullet.h"
#include "grp_defs.h"
#include "sys/proto.h"

#define	MAX_SHFL_AMOUNT	900	/* Max amount to shuffle in one blow */
/* The following two parameters decide whether to shuffle at all */
#define	MIN_SHFL_AMOUNT	900	/* Min amount to start shuffling */
#define MIN_PART_OF	20	/* Or give receiver more then 1/MIN_PART_OF
				 * of receiver's free inode's */

extern	Inode *		Inode_table;	/* pointer to start of inode table */
extern	Superblk	Superblock;

extern	Inodenum	bs_local_inodes_free;		/* number of free inodes of this owner */


static inode_transfer(member,count)
	int		member ;
	Inodenum	count ;
{
	/* Transfer control of count inodes to member */
	register Inode *	ip;	/* inode pointer */
	Inodenum		inode;	/* The number of the inode */

	dbmessage(5,("starting to shuffle %ld inodes to new owner %d",
			(long)count,member));
	for (ip = Inode_table + Superblock.s_numinodes - 1;
	     ip >= Inode_table + 1; ip--) {
		/* First check whether inode is free, if so, lock and retry*/
		if ( !(ip->i_flags&(I_ALLOCED)) &&
		     ip->i_owner==Superblock.s_member) {
			inode= ip - Inode_table ;
			if ( trylock_inode(inode, L_WRITE) ) {
				/* Failure */
				continue ;
			}
			if ( !(ip->i_flags&I_ALLOCED) &&
			     ip->i_owner==Superblock.s_member) {
				/* Now we are sure it's free */
				
				/* Upgrade version number for the last time */
				dbmessage(89,("Moving %ld to %d",inode,member));
				lock_ino_cache(inode) ;
				bs_version_incr(ip); 
				ip->i_owner=member ;
				writeinode(ip,ANN_DELAY|ANN_ALL,SEND_CANWAIT) ;
				bs_local_inodes_free-- ;
				count-- ; 
			}
			unlock_inode(inode) ;
			if ( count <=0 ) return ; 
		}
	}
}


void
bs_shuffle_free()
{
	Inodenum	mean_free ;
	Inodenum	total_free ;
	Inodenum	my_free ;
	Inodenum	i_count ;
	int		member ;
	int		receiving_member ;
	Inodenum	receiving_free ;
	int		members_up;
	Inodenum	his_free ;
	Superblk	SuperCopy ;
	
	
	my_free= bs_local_inodes_free ;

	receiving_member= -1 ;
	receiving_free= my_free ;
	members_up=0 ;
	total_free=0 ;

	super_lock();
	SuperCopy= Superblock ;
	super_free();

	for ( member=0 ; member<S_MAXMEMBER ; member++ ) {
		his_free= bs_grp_blocks(member) ;
		if ( SuperCopy.s_memstatus[member].ms_status==MS_ACTIVE &&
		     SuperCopy.s_member!=member &&
		     his_free!=0) {
			if ( grp_member_present(member) ) {
				members_up++ ;
				his_free=bs_grp_ino_free(member) ;
				total_free += his_free ;
				if ( his_free<receiving_free ) {
					receiving_member= member ;
					receiving_free= his_free ;
				}	
			}
		}
	}
	if ( receiving_member == -1 ) return ;

	dbmessage(25,("Total free= %ld, free= %ld, member_up %d",
		total_free,my_free,members_up)) ;

	if ( members_up == 0 ) return ;
	mean_free= total_free/members_up + 2 ;

	if ( my_free < mean_free ) {
		return ;
	}

	/* Calculate difference between my_free and his_free */
	i_count= (my_free - receiving_free)/2 ;
	/* Do not get lower than mean */
	if ( my_free - i_count < mean_free ) {
		i_count= my_free - mean_free ;
	}
	/* Now i_count holds the number of inodes to transfer to
	   receiving member */
	/* Now shuffle only if we have more than MIN_SHFL_AMOUNT inodes or
	   the increase is more then 1/MIN_PART_OF of the receiving member */
	if ( i_count<MIN_SHFL_AMOUNT && MIN_PART_OF*i_count<=receiving_free ) {
		return ;
	}

	/* Slowly but surely, avoids stress when adding new members */
	if ( i_count>MAX_SHFL_AMOUNT ) i_count=MAX_SHFL_AMOUNT ;

	/* Send the stuff over */

	inode_transfer(receiving_member,i_count) ;
	bs_send_resources(SEND_CANWAIT) ;
	bs_publist(SEND_CANWAIT) ;
}
