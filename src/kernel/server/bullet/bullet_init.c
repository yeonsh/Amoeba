/*	@(#)bullet_init.c	1.9	96/02/27 14:12:27 */
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
 */

#include "amoeba.h"
#include "byteorder.h"
#include "machdep.h"
#include "module/mutex.h"

#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "alloc.h"
#include "cache.h"
#include "stats.h"
#include "bs_protos.h"
#include "sys/proto.h"

vir_bytes	seg_maxfree();	/* find size of largest free memory segment */

/* Convert a size in bytes (blocks) to megabytes, rounded up */
#define	MEG_SHIFT		20
#define	BYTES_TO_MEG(b)		((unsigned) (((b) + (1 << (MEG_SHIFT - 1))) >> MEG_SHIFT))
#define	BLOCKS_TO_MEG(b, bsz)	((unsigned) ((b) / ((1 << MEG_SHIFT) / (bsz))))

/* In-core copy of superblock from disk */
Superblk	Superblock;

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

	if (ip->i_lifetime != 0)	/* then inode in use */
	{
	    /*
	     * At boot time we mark all in use inodes as maximum lifetime
	     * since we only keep a "non-zero" lifetime on disk.
	     */
	    ip->i_lifetime = MAX_LIFETIME;
	    addr = ip->i_dskaddr;
	    DEC_DISK_ADDR_BE(&addr);
	    size = ip->i_size;
	    DEC_FILE_SIZE_BE(&size);
	    if (size > Largest_file_size)
	    {
		bwarn("build_free_list: inode %ld too big for cache: size = %ld\n", i, size);
	    }
	    if (size != 0)	/* empty file has no disk blocks and is ok */
	    {
		numblocks = NUMBLKS(size);
		if (addr <= Superblock.s_inodetop ||
		    addr > Superblock.s_numblocks ||
		    size < 0 ||
		    addr + numblocks > Superblock.s_numblocks)
		{
		    bwarn("build_free_list: Clearing bad inode %ld, size %ld, addr %ld\n", i, size, addr);
		    lock_inode(i, L_WRITE);
		    destroy_inode(i);
		}
		else  /* inode is valid, put resources on in-use lists */
		{
		    if (a_used(A_DISKBLOCKS,
				(Res_addr) addr, (Res_size) numblocks) < 0)
		    {
			bwarn("build_free_list: Duplicate block(s). Inode %ld removed\n", i);
			lock_inode(i, L_WRITE);
			destroy_inode(i);
		    }
		}
	    }
	    else /* disk address should be null since no blocks! */
		if (addr != 0)
		{
		    bwarn("build_free_list: Clearing bad empty file %ld, size %ld, addr %ld\n", i, size, addr);
		    lock_inode(i, L_WRITE);
		    destroy_inode(i);
		}
	}
    } /* end for */

    if (a_high_fragmentation(A_DISKBLOCKS))
	bwarn("heavy disk fragmentation\n");
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
    long	free;		/* # units of resource still free */
    vir_bytes	size;
    capability	pub_cap;

    /* Check that inode size is sensible - otherwise the compiler is odd */
    if (sizeof (Inode) != INODE_SIZE)
    {
	bwarn("Inode size is %d, should be %d\nServer is inactive!\n",
						sizeof(Inode), INODE_SIZE);
	return 0;
    }

    /* Figure out which vdisk to use and read the superblock */
    if (get_disk_server_cap() == 0)
	return 0;

    /* Now that we know there is a disk we can announce ourselves */
    printf("BULLET SERVER INITIALIZATION\n\n");

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
     * of memory (>= 32Mb :-) is involved. However, since kernel threads are
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
    if (a_begin(A_DISKBLOCKS, (Res_addr) Superblock.s_inodetop + 1,
				    (Res_addr) Superblock.s_numblocks) < 0)
    {
	bpanic("Cannot begin resource allocator for disk blocks.\n");
    }

    if (a_begin(A_CACHE_MEM, (Res_addr) Bc_begin, (Res_addr) Bc_end) < 0)
	bpanic("Cannot begin resource allocator for buffer cache.\n");

    /* Get the inode table and start inode allocator */
    inode_init();

    /* Initialise buffer cache management */
    cache_init();

    Largest_file_size = (b_fsize) a_notused(A_CACHE_MEM);

    /* Work out which inodes and disk blocks are in use. */
    build_free_list();

    /* Print some cheery statistics about the current state of the server */
    printf("Buffer cache size = 0x%lx bytes\n", Bc_end - Bc_begin);
    printf("Largest possible file size = 0x%lx bytes\n", Largest_file_size);
    free = num_free_inodes();
    printf("%ld inodes free, %ld inodes used\n",
				free, Superblock.s_numinodes - free - 1);
    if ((free = a_notused(A_DISKBLOCKS)) < 0)
	bpanic("Defective diskblock free list\n");
    printf("%ld disk blocks free, %ld disk blocks used\n",
				free, Superblock.s_numblocks - free);

    /*
     * Register the bullet server's supercap - first make it a put
     * capability
     */
    priv2pub(&Superblock.s_supercap.cap_port, &pub_cap.cap_port);
    pub_cap.cap_priv = Superblock.s_supercap.cap_priv;
    dirappend(BULLET_SVR_NAME, &pub_cap);
    return 1;
}
