/*	@(#)inode.c	1.8	96/02/27 14:12:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * INODE.C
 *
 *	This routine manages allocation and freeing of inodes.
 *	An inode is free if the lifetime field of the inode is 0.
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

#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "alloc.h"
#include "cache.h"
#include "preempt.h"
#include "bs_protos.h"
#include "sys/proto.h"

#if !defined(USER_LEVEL) && !defined(NDEBUG)
#include "sys/global.h"
#include "exception.h"
#include "machdep.h"
#include "sys/kthread.h"
#endif

extern	Inode *		Inode_table;	/* pointer to start of inode table */
extern	Superblk	Superblock;
extern	b_fsize		Blksizemin1;

static	Inode *		Last_free;	/* last inode that was allocated */
static	Inode *		Limit;		/* pointer to end of inode table */
/*
 * The following are used to implement counting locks.
 */
static	mutex *		Inode_lock;	/* table of locks for inodes */

#if !defined(USER_LEVEL) && !defined(NDEBUG)
struct thread ** ILKCHK;
#endif


/*
 * INODE_INIT
 *
 *	Set Limit and set the Last_free pointer at some reasonable
 *	position: for the sake of efficiency the first free inode.
 *	This routine is run at startup.  It marks allocated inodes for
 *	later use by the allocation mechanism.
 */

void
inode_init()
{
    register Inode *	ip;
    register Inode *	lim;
    Res_size		numbytes;
    Inodenum		i;
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

    numbytes = Superblock.s_numinodes * sizeof (mutex);
    CEILING_BLKSZ(numbytes);
    Inode_lock = (mutex *) a_alloc(A_CACHE_MEM, numbytes);
    if (Inode_table == 0 || Inode_lock == 0)
	bpanic("Insufficient memory for inode table cache.");

    /* Initialize the locks to free */
    for (i = 0; i < Superblock.s_numinodes; i++)
	mu_init(Inode_lock + i);

#if !defined(USER_LEVEL) && !defined(NDEBUG)

    numbytes = sizeof (struct thread *) * Superblock.s_numinodes;
    CEILING_BLKSZ(numbytes);
    ILKCHK = (struct thread **) a_alloc(A_CACHE_MEM, numbytes);
    if (ILKCHK == 0)
	bpanic("Insufficient memory for inode cross-check table.");
    (void) memset((_VOIDSTAR) ILKCHK, 0, (size_t) numbytes);

#endif

    read_inodetab(inodeblks);

    lim = Limit = Inode_table + Superblock.s_numinodes;
    Last_free = 0;

    /*
     * Go through the inodes and mark those that are allocated (in use).
     * Remember the first free inode.
     */
    for (ip = Inode_table + 1; ip < lim; ip++)
    {
	/*
	 * Files with the intent flag still on were never written to disk
	 * so mark them as empty.
	 */
	if (ip->i_flags & I_INTENT)
	    ip->i_lifetime = 0;

	if (ip->i_lifetime != 0)
	    ip->i_flags = I_ALLOCED;
	else
	    ip->i_flags = 0;
	if (Last_free == 0 && ip->i_flags == 0)
	    Last_free = ip;
    }

    /*
     * See if there are any inodes.
     * If not then we still need to have last_free pointing at the inode table!
     */
    if (Last_free == 0)
    {
	Last_free = Inode_table + 1;
	bwarn("no free inodes!\n");
	return;
    }
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
    register mutex *	lp;

    assert(inode > 0);
    assert(inode < Superblock.s_numinodes);

    lp = Inode_lock + inode;
#if !defined(USER_LEVEL) && !defined(NDEBUG)
    DISABLE_PREEMPTION;
    assert(curthread != 0);
    if (*lp == 0)
	assert(*(ILKCHK+inode) == 0);
#endif
    mu_lock(lp);
#if !defined(USER_LEVEL) && !defined(NDEBUG)
    *(ILKCHK+inode) = curthread;
    ENABLE_PREEMPTION;
#endif
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
    assert(inode > 0);
    assert(inode < Superblock.s_numinodes);

    DISABLE_PREEMPTION;
    mu_unlock(Inode_lock + inode);
#if !defined(USER_LEVEL) && !defined(NDEBUG)
    if (*(ILKCHK+inode) != curthread)
	bpanic("unlock_inode: %x trying to unlock inode of %x\n",
						*(ILKCHK+inode), curthread);
    *(ILKCHK+inode) = 0;
#endif
    ENABLE_PREEMPTION;
}


/*
 * ALLOC_INODE
 *
 *	This routine allocates an inode from the pool of free inodes.  If
 *	none are free it returns -1.  Otherwise it generates a new random
 *	number for the capability.  NB. an inode is free if the i_flags
 *	field is 0.  We never allocate inode 0 since the supercap for the
 *	server has object #0 and the inode number is the object number in
 *	the file capability.
 */

Inodenum
alloc_inode()
{
    register Inode *	ip;	/* inode pointer */
    register mutex *	lp;	/* lock pointer */

    /*
     * Find a free inode.  Return -1 if none.
     */
    DISABLE_PREEMPTION; /* Protect "Last_free" variable and extant locks */
    ip = Last_free;
    lp = Inode_lock + (ip - Inode_table);
    while ((ip->i_flags & I_ALLOCED) || *lp != 0) /* Not free! */
    {
	lp++;
	if (++ip >= Limit)	/* wrap around at end of inode table */
	{
	    ip = Inode_table + 1;
	    lp = Inode_lock + 1;
	}
	if (ip == Last_free)	/* are we back where we started? */
	{
	    ENABLE_PREEMPTION;
	    return -1;
	}
    } 
    Last_free = ip;

    /*
     * Generate a new random number and initialise other fields.
     * We must mark the inode as unused until file is committed so lifetime is
     * set to 0.  We record in the in-core flags that the inode is in use.
     */
    assert(*lp == 0);
    lock_inode((Inodenum) (lp - Inode_lock), L_WRITE);
    ip->i_flags = I_ALLOCED;
    ip->i_lifetime = 0;
    ip->i_dskaddr = 0;
    ip->i_size = 0;
    /* The following are unnecessary since they are set to 0
    ENC_DISK_ADDR_BE(&ip->i_dskaddr);
    ENC_FILE_SIZE_BE(&ip->i_size);
    */
    uniqport(&ip->i_random);
    ENABLE_PREEMPTION;
    return ip - Inode_table;
}


/*
 * FREE_INODE
 *
 *	Release an inode to the free list.
 *	Assume the disk blocks have already been freed.
 */

void
free_inode(inode)
Inodenum	inode;
{
    static port	nullport;	/* need to zero the random number! */
    Inode *	ip;

    ip = Inode_table + inode;

    assert(Inode_lock[inode] != 0); /* Make sure that the lock is on! */
    assert(ip->i_flags & I_ALLOCED);
    assert(inode > 0);
    assert(inode < Superblock.s_numinodes);

    ip->i_dskaddr = 0;
    ip->i_lifetime = 0;
    ip->i_size = 0;
    ip->i_flags = 0;
    ip->i_random = nullport;
    /* The following are unnecessary since they are set to 0
    ENC_DISK_ADDR_BE(&ip->i_dskaddr);
    ENC_FILE_SIZE_BE(&ip->i_size);
    */
    /* Write it out to disk if the disk version was ever valid */
    writeinode(ip);
    unlock_inode(inode);
}


/*
 * NUM_FREE_INODES
 *
 *	Counts the number of unused inodes.  This is purely for printing
 *	nice statistics for the administrator.  It counts the allocated but
 *	uncommitted inodes as free.  If you don't like that then test the
 *	i_flags field instead of the i_lifetime field.
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
	if (ip->i_lifetime == 0)
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
    if (Inode_lock[inode] == 0) /* Naughty - looking inside a mutex! */
	return 0;
    else
	return 2;
}


#ifndef NDEBUG

int
inode_locked_by_me(inode)
Inodenum	inode;
{
    int x = inode_locked(inode);

#if !defined(USER_LEVEL) && !defined(NDEBUG)
    if (x == 2 && *(ILKCHK+inode) != curthread)
	bpanic("inode_locked_by_me: fail for inode %d, cur=%x, holder=%x\n",
					    inode, curthread, *(ILKCHK+inode));
#endif
    return x;
}

#endif /* NDEBUG */


/*
 * DESTROY_INODE
 *
 *	This routine rids the system of the named inode and cleans up any
 *	cache or disk resources associated therewith.
 */

void
destroy_inode(inode)
Inodenum	inode;
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
    addr = ip->i_dskaddr;
    DEC_DISK_ADDR_BE(&addr);
    if (addr)
    {
	Res_size blocks;

	/* Free disk blocks */
	size = ip->i_size;
	DEC_FILE_SIZE_BE(&size);
	blocks = (Res_size) NUMBLKS(size);
	if (a_free(A_DISKBLOCKS, (Res_addr) addr, blocks) < 0)
	    bpanic("destroy_inode: a_free of diskblocks failed");
    }
    free_inode(inode);
}
