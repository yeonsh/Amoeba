/*	@(#)bullet_init.c	1.1	96/02/27 14:07:04 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * BULLET_INIT.C
 *
 *	This file contains the declarations of all non-static global
 *	variables used in the bullet server plus the routines
 *	which initialize the server.
 *
 *  Author:
 *	Greg Sharp, Jan 1989
 *  Modified:
 *	Ed Keizer, 1995 - to fit in group bullet
 */

#include "amoeba.h"
#include "byteorder.h"
#include "machdep.h"
#include "module/mutex.h"
#include "semaphore.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "alloc.h"
#include "cache.h"
#include "stats.h"
#include "bs_protos.h"
#include "grp_defs.h"
#ifdef USER_LEVEL
#include "thread.h"
#else
#include "sys/proto.h"
#include "sys/kthread.h"
#endif

vir_bytes	seg_maxfree();	/* find size of largest free memory segment */

/* Convert a size in bytes (blocks) to megabytes, rounded up */
#define	MEG_SHIFT		20
#define	BYTES_TO_MEG(b)		((unsigned) (((b) + (1 << (MEG_SHIFT - 1))) >> MEG_SHIFT))
#define	BLOCKS_TO_MEG(b, bsz)	((unsigned) ((b) / ((1 << MEG_SHIFT) / (bsz))))

extern	Inodenum	bs_local_inodes_free;
				/* number of free inodes of this owner */

extern int		bs_log_msgs ;
extern int		bs_print_msgs ;
extern int     		bs_debug_level ;

/* In-core copy of superblock from disk */
Superblk	DiskSuper;
/* Decoded copy of same */
Superblk	Superblock;
/* Write lock on the super block, just here to prevent unlikely
 * nightmares.
 * Under two conditions the super block can change.
 * First, if a new member is introduced the new member initializes
 * most superblock elements. This lock serves to protect this happening
 * by two threads simultaneously. The `real' service has not started then.
 * Second, when a member changes state. This can happen at any time.
 * This lock prevents two threads updating the Superblock simultaneously.
 * Routines using the member states must be prepared to have things
 * changes under their hands.
 */
static mutex	SuperLock;

/* Putport version of file getport */
port		FilePutPort;

/* Pointer to in-core copy of inode table */
Inode *		Inode_table;

/* Repository of bullet server usage statistics since boottime */
Statistics	Stats;

/* Start and finish of Buffer_cache */
bufptr		Bc_begin;
bufptr		Bc_end;

/*
 * The size of the largest possible file that can fit in the cache after
 * we have allocated memory for the inodes and the cache slots.
 * Therefore it is a limit on the largest file that can be read/created.
 */
b_fsize		Largest_file_size;

/*
 * Because of the need to use the block size in bytes minus 1 from time to time
 * we precompute it and store it here.  It is filled in by valid_superblock().
 */
b_fsize		Blksizemin1;

disk_addr	bs_super_size;	/* The size of the super block in
				 * local blocks.
				 */

/*
 * BUILD_FREE_LIST
 *
 *	This routine goes through all the inodes in core and builds up a list
 *	of which disk blocks are in free.  It only bothers to lock things
 *	when it has to call destroy_inode since we are effectively single
 *	threaded at this point.
 */

static void
build_free_list()
{
    Inodenum	i;
    Inode *	ip;
    disk_addr	addr;
    b_fsize	size;
    disk_addr	numblocks;

    for (ip = Inode_table+1, i = 1; i < Superblock.s_numinodes; ip++, i++)
    {
#ifndef USER_LEVEL
	/*
	 * Because we might run for a long time we should give the
	 * interrupt queue a chance to drain by rescheduling ourselves
	 * after each 2048 inodes
	 */
	if (!(i & 2047))
	    threadswitch();
#endif

	if (ip->i_flags&I_INUSE)	/* then inode in use */
	{
	    addr = ip->i_dskaddr;
	    DEC_DISK_ADDR_BE(&addr);
	    size = ip->i_size;
	    DEC_FILE_SIZE_BE(&size);
	    if (size > Largest_file_size)
	    {
		bwarn("build_free_list: inode %ld too big for cache: size = %ld", i, size);
	    }
	    /* empty or not present file has no disk blocks and is ok */
	    if (size != 0 ) {
		if (ip->i_flags&I_BUSY) {
		    ip->i_flags &= ~I_BUSY ; /* Incore only flag */
		}
		if (ip->i_flags&I_PRESENT) {
		    if ( ip->i_flags&I_INTENT ) {
			/* On disk inode without file contents. Destroy. */
			bwarn("destroying premature inode %ld",i) ;
                        lock_inode(i, L_WRITE);
                        destroy_inode(i,0,ANN_NEVER,SEND_CANWAIT);
			continue ;
		    }
                    numblocks = NUMBLKS(size);
                    if (addr <= Superblock.s_inodetop ||
                        addr > Superblock.s_numblocks ||
                        size < 0 ||
                        addr + numblocks > Superblock.s_numblocks)
                    {
                        bwarn("build_free_list: Clearing contents of bad inode %ld, size %ld, addr %ld", i, size, addr);
                        lock_inode(i, L_WRITE);
                        destroy_contents(i,0);
                    }
                    else  /* inode is valid, put resources on in-use lists */
                    {
                        if (a_used(A_DISKBLOCKS,
                                    (Res_addr) addr, (Res_size) numblocks) < 0)
                        {
                            bwarn("build_free_list: Duplicate block(s). Contents of node %ld removed", i);
                            lock_inode(i, L_WRITE);
                            destroy_contents(i,0);
                        }
                    }
                }
	    } else /* disk address should be null since no blocks! */
		if (addr != 0)
		{
		    bwarn("build_free_list: Clearing contents of bad empty file %ld, size %ld, addr %ld", i, size, addr);
		    lock_inode(i, L_WRITE);
		    destroy_contents(i,0);
		}
	}
    } /* end for */

    if (a_high_fragmentation(A_DISKBLOCKS))
	bwarn("heavy disk fragmentation");
}


/*
 * BULLET_INIT
 *
 *	This does the initialization work for the bullet_server.
 *	It allocates the buffer cache, reads the superblock and inodes into
 *	core, builds the free lists and prints some encouragement on the
 *	console.  If there is no vdisk with a file system it returns 0.
 *	If anything isn't correct it panics.  If all is well it returns 1
 *	It is always called before any server threads run and so is
 *	guaranteed to run exactly once.
 */

int
bullet_init()
{
    vir_bytes	size;
    peer_bits	test_var ;

    /* Initialize to logging */
    bs_log_msgs=1 ;
#ifndef USER_LEVEL    
    bs_print_msgs = 0;
#endif
    bs_debug_level = DEBUG_LEVEL ;

    /* Check that inode size is sensible - otherwise the compiler is odd */
    if (sizeof (Inode) != INODE_SIZE)
    {
	bwarn("Inode size is %d, should be %d\nServer is inactive!",
						sizeof(Inode), INODE_SIZE);
	return 0;
    }

    /* Check that the peer bitmap can contain all members */
    test_var=1 ;
    test_var <<= (S_MAXMEMBER-1) ;
    if ( test_var == 0 || test_var!= (1<<(S_MAXMEMBER-1)) ) {
	bwarn("type \"peer_bits\" is too small\nServer is inactive!");
	return 0;
    }

    
    /* Figure out which vdisk to use and read the superblock */
    if (get_disk_server_cap() == 0)
	return 0;

    /* Now that we know there is a disk we can announce ourselves */
    message("BULLET SERVER INITIALIZATION");

    /*
     * Set up pointers to beginning and end of buffer cache.
     * Make sure that our buffer cache is block aligned!  The inode table
     * goes at the start of the cache memory and relies on blocksize
     * alignment.
     * NB: We must leave some memory free for other kernel threads and
     *     possible user processes (such as soap).  It leaves
     *     BS_MEM_RESERVE bytes if no fixed cache size is defined.
     */
#ifdef BULLET_MEMSIZE
    size = BULLET_MEMSIZE;
#else
    size = seg_maxfree();
    if (size < BS_MEM_RESERVE)
    {
	bpanic(
	  "bullet_init: memory reserve (0x%x) exceeds free memory (0x%x)\n",
	    BS_MEM_RESERVE, size);
    }
    size -= BS_MEM_RESERVE;
#endif /* BULLET_MEMSIZE */

    /*
     * The following allocate may take a long time, especially when lots
     * of memory (32Mb :-) is involved. However, since kernel threads are
     * not preempted the enqueued interrupt routines never get a chance to
     * run, and eventually the interrupt queue will overflow causing a
     * panic.
     */
#ifndef USER_LEVEL
    disable();
#endif /* USER_LEVEL */
    Bc_begin = (bufptr) aalloc(size, (int) Blksizemin1 + 1);
#ifndef USER_LEVEL
    enable();
#endif /* USER_LEVEL */

    Bc_end = Bc_begin + size;
    if (Bc_begin >= Bc_end)
	bpanic("bullet_init: no buffer cache");

    /* Initialise resource allocation.  NB: cache_mem is initially free! */
    a_init(BYTES_TO_MEG(Bc_end - Bc_begin) +
	   BLOCKS_TO_MEG(Superblock.s_numblocks, Blksizemin1 + 1));
    if (a_begin(A_CACHE_MEM, (Res_addr) Bc_begin, (Res_addr) Bc_end) < 0)
	bpanic("Cannot begin resource allocator for buffer cache.\n");

    /* Initialise buffer cache management */
    cache_init();

    Largest_file_size = (b_fsize) a_notused(A_CACHE_MEM);

    /* Print some cheery statistics about the current state of the server */
    message("Buffer cache size = 0x%lx bytes", Bc_end - Bc_begin);
    message("Largest possible file size = 0x%lx bytes",Largest_file_size);

    /* Calculate the size of the super struct in local blocks */
    /* It is the of disk blocks into a D_PHYSBLK */
    if ( D_PHYS_SHIFT<=Superblock.s_blksize ) {
	/* Super info takes one block or less */
	bs_super_size= 1 ;
   } else {
	/* To be honest, bs_supersize will always be one, because
	 * D_PHYS_SHIFT seems to be the minimum.
         */
	bwarn("Super block takes more then one block") ;
	bs_super_size= 1 << (D_PHYS_SHIFT-Superblock.s_blksize) ;
    }

    /* Super Block Lock */
    mu_init(&SuperLock) ;

    /* Init group structures */
    bs_grp_init() ;

    return 1 ;
}


int
bullet_start()
{
    long	free;		/* # units of resource still free */

    /* Now that we can start ... */
    message("BULLET SERVER STARTUP");

    if (a_begin(A_DISKBLOCKS, (Res_addr) Superblock.s_inodetop + 1,
				    (Res_addr) Superblock.s_numblocks) < 0)
    {
	bpanic("Cannot begin resource allocator for disk blocks.\n");
    }

    /* Convert file get-port to put-port */
    priv2pub(&Superblock.s_fileport,&FilePutPort) ;

    /* Get the inode table and start inode allocator */
    /* Could have been done by setinodes() as a result of a welcome */
    if ( !Inode_table) inode_init();

    /* Work out which inodes and disk blocks are in use. */
    build_free_list();

    message("%ld inodes free, %ld inodes used",
				bs_local_inodes_free,
				- 1);
    if ((free = a_notused(A_DISKBLOCKS)) < 0) {
	bpanic("Defective diskblock free list\n");
    }
    message("%ld disk blocks free, %ld disk blocks used",
				free, Superblock.s_numblocks - free);
    return 1 ;
}

void
super_lock() {
	mu_lock(&SuperLock) ;
}

void
super_free() {
	mu_unlock(&SuperLock) ;
}
