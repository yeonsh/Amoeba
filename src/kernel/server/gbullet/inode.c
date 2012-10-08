/*	@(#)inode.c	1.1	96/02/27 14:07:27 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * INODE.C
 *
 *	This routine manages allocation and freeing of inodes.
 *	Inodes can have two ways of being in use. If I_INUSE
 *	is set the rest of the inode's fields contain valid data
 *	for a committed file. If I_ALLOCED is set the inode is
 *	in use, but might contain garbage.
 *	If I_DESTROY is set the inode has been destroyed, but we
 *	are not sure whether this fact has been recorded on all
 *	our fellow members.
 *	An inode on disk is free if I_INUSE or I_DESTROY are not set and this
 *	server is the owner of the inode. I_INUSE & I_DESTROY exclude each
 *	other. 
 *	An inode in core is free if I_ALLOCED is not set.
 *	With I_PRESENT set the file is on the disk.
 *	Note that Superblock must have been read before this code
 *	can run.
 *	NOTE WELL:  The locking mechanism for inodes implies that there
 *		    must never be more than 127 threads in a bullet server.
 *		    To increase this change the locks from char to int and
 *		    watch the alignment!
 *
 * Author:
 *	Greg Sharp, Jan 1989
 * Modification:
 *	Greg Sharp, Jan 1992 -	rewritten with inode locking
 *	Greg Sharp, May 1992 -	replaced code that only works in the kernel
 *				with a "temporary" hack which does not
 *				implement read locks.
 *	Ed Keizer, 1995      -  to fit in group bullet
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
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "alloc.h"
#include "cache.h"
#include "preempt.h"
#include "bs_protos.h"
#include "sys/proto.h"
#include "event.h"
#include "grp_bullet.h"
#include "grp_defs.h"


#if !defined(USER_LEVEL) && !defined(NDEBUG)
#include "sys/global.h"
#include "exception.h"
#include "machdep.h"
#include "sys/kthread.h"
#endif

/* Invalid flags when inode is not in use */
#define I_NOT_INUSE	(I_PRESENT)

extern	Inode *		Inode_table;	/* pointer to start of inode table */
extern	Superblk	Superblock;
extern	b_fsize		Blksizemin1;

static	Inode *		Last_free;	/* last inode that was allocated */
static	Inode *		Limit;		/* pointer to end of inode table */

	Inodenum	bs_local_inodes_free;		/* number of free inodes of this owner */


/* Pointer to bits indicating replication, locked with lock_inode() */
peer_bits		*Inode_reptab;

/* Routines for inode allocation, called by set_inodes and inode_init.
 */

static disk_addr
in_alloc()
{
    Res_size		numbytes;
    disk_addr		inodeblks;

    /*
     * We allocate space at the beginning of the cache memory for the in-core
     * inode table.
     *  NOTE WELL:	The cache memory must be properly aligned so that
     *			write_inode works properly.  Therefore we round
     *			up to blocksize.
     */

    numbytes = Superblock.s_numinodes * sizeof (Inode);
    /* Round up to blocksize */
    numbytes += Blksizemin1;
    numbytes &= ~Blksizemin1;
    inodeblks = numbytes >> Superblock.s_blksize;
    Inode_table = (Inode *) a_alloc(A_CACHE_MEM, numbytes);

    numbytes = Superblock.s_numinodes*sizeof(peer_bits);
    CEILING_BLKSZ(numbytes);
    Inode_reptab = (peer_bits *) a_alloc(A_CACHE_MEM, numbytes);
    memset((_VOIDSTAR)Inode_reptab,0,(size_t)numbytes);

    numbytes = Superblock.s_numinodes * sizeof (mutex);
    CEILING_BLKSZ(numbytes);
    typed_lock_init() ;

    if (Inode_table == 0 )
	bpanic("Insufficient memory for inode table cache.");

    Limit = Inode_table + Superblock.s_numinodes;
    Last_free = 0;

    return inodeblks ;
}

/*
 * INODE_INIT
 *
 *	Set Limit and set the Last_free pointer at some reasonable
 *	position: for the sake of efficiency the first free inode.
 *	This routine is run at startup.  It marks allocated inodes for
 *	later use by the allocation mechanism.
 *	It also allocates the Time To Live array.
 */

void
inode_init()
{
    disk_addr		inodeblks;
    register Inode *	ip;
    register Inode *	lim;
    register peer_bits * rep_ip ;
    register peer_bits	my_mask ;

    inodeblks= in_alloc() ;

    read_inodetab(inodeblks);

    lim = Limit;
    bs_local_inodes_free = 0 ;
    my_mask= 1<<Superblock.s_member ;
    /*
     * Go through the inodes and mark those that are allocated (in use).
     * Remember the first free inode.
     */
    for (ip =Inode_table+1, rep_ip=Inode_reptab+1; ip < lim; ip++, rep_ip++)
    {
	if ( ip->i_flags & I_INTENT ) {
	    bwarn("Inode %8ld: alloced intent",ip-Inode_table) ;
	}
	/* Clear in-core only flags */
	ip->i_flags &= ~(I_ALLOCED);
	if ( ip->i_flags&(I_INUSE|I_DESTROY) ) {
	    ip->i_flags |= I_ALLOCED ;
	    if ( ip->i_flags&I_PRESENT ) *rep_ip= my_mask ;
	} else {
	    if ( ip->i_flags & I_NOT_INUSE ) {
		bwarn("Inode %8ld: status %x",ip-Inode_table,ip->i_flags) ;
		ip->i_flags &= ~I_NOT_INUSE ;
	    }
	    if ( ip->i_owner == Superblock.s_member ) {
		bs_local_inodes_free++ ;
		if ( Last_free == 0 ) {
		    Last_free = ip;
		}
	    }
	}
    }

    /*
     * See if there are any inodes.
     * If not then we still need to have last_free pointing at the inode table!
     */
    if (Last_free == 0)
    {
	Last_free = Inode_table + 1 ;
	bwarn("no free inodes!");
    }
    bs_init_irw() ;
    gs_init_ttl() ;
}

/* Inode_init
 * Called when a fresh slave has just heard how many inodes there are.
 * It sets all its inodes to owner `unknown'.
 * This inode table is written to disk.
 * This Routine intializes the incore tables, like inode_init.
 */
void
set_inodes()
{
    register Inode *	ip;
    register Inode *	lim;
    disk_addr		inodeblks;

    inodeblks= in_alloc() ;

    lim = Limit;
    /* First set all entries to zero */
    memset((_VOIDSTAR)Inode_table,0,
		(size_t)inodeblks*Superblock.s_blksize) ;

    /*
     * Go through the inodes and mark all as unknown.
     */
    for (ip = Inode_table + 1; ip < lim; ip++)
    {
	ip->i_owner=S_UNKNOWN_ID ;
    }

    write_inodetab(inodeblks);

    /*
     * Need to have last_free pointing at the first usable inode !
     */
    Last_free = Inode_table + 1;
    bs_local_inodes_free= 0 ;
    bs_init_irw() ;
    gs_init_ttl() ;
}

/*
 * LOCK_INODE
 *
 *	Lock the specified inode.  Go to sleep if it is already locked.
 *	No fairness guarantees here either!
 */

/*ARGSUSED*/
void
lock_inode(inode, locktype)
Inodenum	inode;
int		locktype; /* ignored in this version */
{
    typed_lock(INODE_LCLASS,(long)inode) ;
}


/*
 * TRYLOCK_INODE
 *
 *	Lock the specified inode.   if it is already locked.
 *	No fairness guarantees here either!
 */

/*ARGSUSED*/
int
trylock_inode(inode, locktype)
Inodenum	inode;
int		locktype; /* ignored in this version */
{
    int		retval ;

    retval=  typed_trylock(INODE_LCLASS,(long)inode) ;
    return retval ;
}


/*
 * UNLOCK_INODE
 *
 *	Unlock the specified inode.  It must be locked on entry.
 */

void
unlock_inode(inode)
Inodenum	inode;
{
    typed_unlock(INODE_LCLASS,(long)inode) ;
}


/*
 * FIND_FREE
 *
 *	This routine allocates an inode from the pool of free inodes.  If
 *	none are free it returns -1.  Otherwise it sets the `allocated'
 *	flag and generates a new random number for the capability.
 *	NB. an inode is free if the i_flags field is 0 and the server is
 *	the owner.
 *	We never allocate inode 0 since the supercap for the
 *	server has object #0 and the inode number is the object number in
 *	the file capability.
 */

static Inodenum find_free()
{
    register Inode *	ip;	/* inode pointer */
    Inodenum		inode;	/* The number of the inode */

    /*
     * Find a free inode.  Return -1 if none.
     */
    DISABLE_PREEMPTION; /* Protect "Last_free" variable and extant locks */
    ip = Last_free; inode= Last_free - Inode_table ;
    while (ip->i_flags & (I_ALLOCED) ||
	   ip->i_owner != Superblock.s_member ||
	   typed_trylock(INODE_LCLASS,inode) ) /* Not free! */
    {
	inode++ ;
	if (++ip >= Limit)	/* wrap around at end of inode table */
	{
	    ip = Inode_table + 1;
	    inode=1 ;
	}
	dbmessage(31,("ip 0x%x, Last_free 0x%x, Table 0x%x",ip,Last_free,Inode_table));
	if (ip == Last_free)	/* are we back where we started? */
	{
	    ENABLE_PREEMPTION;
	    return -1;
	}
    } 
    Last_free = ip;

    /*
     * We record in the in-core flags that the inode is in use.
     */
    /* We could lock the inode cache, but we will not, it does no harm... */
    ip->i_flags = I_ALLOCED;
    bs_local_inodes_free-- ;
    ENABLE_PREEMPTION;
    /* Leave disk address and the owner alone */
    ip->i_size = 0;
    /* The following are unnecessary since they are set to 0
    ENC_DISK_ADDR_BE(&ip->i_dskaddr);
    ENC_FILE_SIZE_BE(&ip->i_size);
    */
    uniqport(&ip->i_random);
    return ip - Inode_table;
}


/*
 * ALLOC_INODE
 *
 *	This routine finds an inode from the pool of free inodes.  If
 *	none are free it inquires at its peers to find free inodes. If
 *	this produces no result it returns -1.
 */

Inodenum
alloc_inode()
{
    /*
     * Find a free inode.  Try once to reshuffle if none.
     */
    return find_free() ;
}


/*
 * UPDATE_INODE
 *
 *	A free inode is replaced by something coming from outside.
 *	Assume no disk blocks are present.
 */

void
update_inode(inode,new)
Inodenum	inode;
struct Inode	*new;
{
    Inode *	ip;

    ip = Inode_table + inode;

    assert(test_typed_lock(INODE_LCLASS,(long)inode)); /* Make sure that the lock is on! */
    assert(inode > 0);
    assert(inode < Superblock.s_numinodes);

    lock_ino_cache(inode) ;
    if ( new->i_flags&I_DESTROY ) {
	ip->i_flags = I_DESTROY|I_ALLOCED ;
    } else if ( new->i_flags&I_INUSE ) {
	ip->i_size = new->i_size;
	ip->i_random = new->i_random;
	ip->i_flags = I_INUSE|I_ALLOCED ;
	if ( ip->i_size==0 ) {
	    ip->i_flags |= I_PRESENT ;
	    Inode_reptab[inode] |= (1<<Superblock.s_member) ;
	}
    }
    ENC_FILE_SIZE_BE(&ip->i_size);
    /* Write it out to disk if the disk version was ever valid */
    writeinode(ip,ANN_DELAY|ANN_MINE,SEND_NOWAIT);
}

/*
 * FREE_INODE
 *
 *	Release an inode to the free list.
 *	Assume the disk blocks have already been freed.
 */


void
free_inode(inode,keeplocked,announce,mode)
Inodenum	inode;
int		keeplocked;
int		announce;
int		mode;
{
    static port	nullport;	/* need to zero the random number! */
    Inode *	ip;

    ip = Inode_table + inode;

    assert(test_typed_lock(INODE_LCLASS,(long)inode)); /* Make sure that the lock is on! */
    assert(ip->i_flags & I_ALLOCED);
    assert(inode > 0);
    assert(inode < Superblock.s_numinodes);

    lock_ino_cache(inode) ;
    if ( ip->i_owner==Superblock.s_member ) {
	bs_local_inodes_free++ ;
	/* Upgrade version number. This will lead to destruction
	 * of replicas.
	 * Updating is not useful if called from take_inode, it
	 * will overwrite our new version number.
	 */
	bs_version_incr(ip) ;
	ip->i_flags = 0;
        ip->i_random = nullport;
    } else {
	/* Mark the inode as destroyed and wait for confirmation by owner.
	 */
	ip->i_flags = I_DESTROY;
    }
    ip->i_dskaddr = 0;
    ip->i_size = 0;
    /* The following are unnecessary since they are set to 0
    ENC_DISK_ADDR_BE(&ip->i_dskaddr);
    ENC_FILE_SIZE_BE(&ip->i_size);
    */
    /* Write it out to disk if the disk version was ever valid */
    writeinode(ip,announce,mode);
    gs_clear_ttl(inode) ;
    if ( !keeplocked) unlock_inode(inode);
}


/*
 * NUM_FREE_INODES
 *
 *	Counts the number of unused inodes.  This is purely for printing
 *	nice statistics for the administrator.  It counts the allocated but
 *	uncommitted inodes as free.  If you don't like that then test the
 *	I_ALLOCED field instead of the I_INUSE field.
 *	It locks the Inode data structures so that it gets a clean-ish
 *	picture.
 */

long
num_free_inodes()
{
    register Inode *	ip;
    register long	total;

    DISABLE_PREEMPTION;
    total = 0;
    for (ip = Inode_table + 1; ip < Limit; ip++)
	if ( !(ip->i_flags&I_ALLOCED) && ip->i_owner==Superblock.s_member)
	    total++;
    ENABLE_PREEMPTION;
    return total;
}


/*
 * INODE_LOCKED
 *
 *	Returns 0 if the specified inode is not locked, 1 if it is read-locked
 *	and 2 if it is write-locked.
 *	Since there are no read locks in this version it always returns 0 or 2.
 */

int
inode_locked(inode)
Inodenum	inode;
{
    assert(inode > 0);
    assert(inode < Superblock.s_numinodes);
    if (test_typed_lock(INODE_LCLASS,(long)inode)) /* Naughty - looking inside a mutex! */
	return 2;
    else
	return 0;
}


#ifndef NDEBUG

int
inode_locked_by_me(inode)
Inodenum	inode;
{
    int x = inode_locked(inode);

    return x;
}

#endif /* NDEBUG */


/*
 * DESTROY_INODE
 *
 *	This routine rids the system of the named inode and cleans up any
 *	cache or disk resources associated therewith.
 *	Nowrite is used if destroyed for reason of existance of new version.
 */

void
destroy_inode(inode,keeplocked,announce,mode)
Inodenum	inode;
int		keeplocked;
int		announce ;
int		mode;
{
    Cache_ent * cp;
    Inode *	ip;
    disk_addr	addr;
    b_fsize	size;

    assert(inode_locked(inode) == 2);
    cp = find_cache_entry(inode);
    if (cp != (Cache_ent *) 0)
	destroy_cache_entry(cp);
    ip = Inode_table + inode;
    addr=0 ;
    if (ip->i_flags&I_PRESENT)
    {
	/* Free disk blocks */
	size = ip->i_size;
	DEC_FILE_SIZE_BE(&size);

	if ( size ) {
	    addr = ip->i_dskaddr;
	    DEC_DISK_ADDR_BE(&addr);
	    assert(addr!=0) ;
	}
    }
    free_inode(inode,keeplocked,announce,mode);
    if ( addr ) {
	Res_size blocks;

	/* We are cautious here. Releasing the content blocks
	 * should happen after the new inode is written.
	 */
	blocks = (Res_size) NUMBLKS(size);
	if (a_free(A_DISKBLOCKS, (Res_addr) addr, blocks) < 0)
	    bpanic("destroy_inode: a_free of diskblocks failed");
    }
}

void
destroy_contents(inode,from_fsck)
Inodenum	inode;
int		from_fsck;
{
    Cache_ent * cp;
    Inode *	ip;
    disk_addr	addr;
    b_fsize	size;
    Res_size	blocks;

    assert(inode_locked(inode) == 2);
    assert(inode > 0);
    assert(inode < Superblock.s_numinodes);

    cp = find_cache_entry(inode);
    if (cp != (Cache_ent *) 0)
	destroy_cache_entry(cp);
    ip = Inode_table + inode;
    assert(ip->i_flags & I_ALLOCED);

    /* Destroy bit in replication table */
    Inode_reptab[inode] &= ~(1<<Superblock.s_member) ;

    if ( !(ip->i_flags&I_PRESENT) ) return ;

    /* Free disk blocks */
    size = ip->i_size;
    DEC_FILE_SIZE_BE(&size);
    if ( size==0 ) return ;

    addr = ip->i_dskaddr;
    DEC_DISK_ADDR_BE(&addr);
    assert(addr!=0) ;

    lock_ino_cache(inode) ;
    ip->i_dskaddr = 0;
    ip->i_flags &= ~I_PRESENT ;
    /* The following are unnecessary since they are set to 0
    ENC_DISK_ADDR_BE(&ip->i_dskaddr);
    */
    /* Write it out to disk if the disk version was ever valid */
    if ( from_fsck ) {
	/* Announce disappearance */
	writeinode(ip,ANN_DELAY|ANN_ALL,SEND_CANWAIT);
	if ( ip->i_owner==Superblock.s_member ) {
	    gs_lostrep();
	}
    } else {
	writeinode(ip,ANN_NEVER,SEND_CANWAIT);
	unlock_inode(inode);
    }

	/* We are cautious here. Releasing the content blocks
	 * should happen after the new inode is written.
	 */
    blocks = (Res_size) NUMBLKS(size);
    if (a_free(A_DISKBLOCKS, (Res_addr) addr, blocks) < 0)
	    bpanic("destroy_contents: a_free of diskblocks failed");
}
