/*	@(#)bs_adm_cmds.c	1.1	96/02/27 14:06:55 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * BP_ADM_CMDS.C
 *
 *	This file contains the implementation of the bullet server
 *	administrative commands on the private port.
 *
 * Author:
 *	Ed Keizer
*/

#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#ifdef USER_LEVEL
#include "thread.h"
#else
#include "sys/proto.h"
#include "sys/kthread.h"
#endif
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
#ifdef INIT_ASSERT
INIT_ASSERT
#endif

extern	Superblk	Superblock;	/* in-core copy of superblock */
extern	Statistics	Stats;
extern	Inode *		Inode_table;	/* pointer to in-core inode table */
extern	bufptr		Bc_begin;	/* Start address of buffer cache */
extern	bufptr		Bc_end;		/* End address of buffer cache */
extern	b_fsize		Blksizemin1;
extern	Inodenum	bs_local_inodes_free;
				/* number of free inodes of this owner */

/* Statistics */
extern int     		bs_max_locks_used ;
extern int     		bs_max_locks ;
extern int     		bs_had_ovflw ;


/*
 * BS_FSCK
 *
 *	This does a file system check and repairs everything it can.
 *	The usual solution is throw away anything that looks like it
 *	is causing inconsistencies.  Something nicer probably should
 *	be devised.
 */

errstat
bs_do_fsck()
{
    disk_addr	addr;
    disk_addr	nblks;
    Inode *	ip;
    Inodenum	i;

    Stats.b_fsck++;

#ifndef NDEBUG
    /* check cache consistency */
    cache_check();
#endif /* NDEBUG */

    /* For each inode check consistency */
    for (ip = Inode_table+1, i = 1; i < Superblock.s_numinodes; ip++, i++)
    {
	lock_inode(i, L_WRITE);
	if (ip->i_flags&I_PRESENT)	/* if inode is on current server */
	{
	    addr = ip->i_dskaddr;
	    DEC_DISK_ADDR_BE(&addr);
	    nblks = (disk_addr) ip->i_size;
	    DEC_DISK_ADDR_BE(&nblks);
	    nblks = NUMBLKS(nblks);

	    /* The empty file doesn't need further checking */
	    if (addr == 0 && nblks == 0)
	    {
		unlock_inode(i);
		continue;
	    }
	    /*
	     * IF file data is allocated in the disk inode table OR
	     * it begins after the end of the disk OR
	     * it takes up a negative number of blocks OR
	     * it takes up blocks not on the disk
	     * THEN destroy the file
	     *  NB: we use destroy_contents to ensure that we kill any cache entry.
	     */
	    if (addr <= Superblock.s_inodetop ||
		addr >= Superblock.s_numblocks ||
		nblks < 0 ||
		addr+nblks >= Superblock.s_numblocks)
	    {
		bwarn("fsck: destroying inode %d", i);
#ifndef NDEBUG
		dump_inode(ip);
		(void) dump_cache_for_inode(i);
#endif /* NDEBUG */
		destroy_contents(i,1);
	    }
	}
	unlock_inode(i);
    }
    return STD_OK;
}


/*
 * BS_STATUS
 *
 *	This function fills the given buffer with status
 *	information in the form of an ascii string.
 *	The report should look vaguely like this:
 *
 *			In Use	Free	Holes
 *	Inodes
 *	Cache Memory
 *	Disk Blocks
 *
 *	Committed Files		usage  fragmentation
 *	Uncommitted Files	usage  fragmentation
 *
 *	Free Cache Slots
 *	Free Hole Slots
 *
 *	Cache Hits	Cache Misses	Compactions
 *	Read Misses	Disk Reads	Disk Writes
 *	Timed out files	Aged Files	std_age
 *	std_localcopy	std_remotecopy	std_destroy
 *	std_info	std_restrict	std_touch
 *	b_sync		b_status	b_fsck		
 *	b_size		b_read		b_create
 *	b_modify	b_insert	b_delete
 *	b_disk_compact	moved files
 *
 *	The status information should be less than 1K so we put it on the
 *	stack.  This routine must do a putrep before returning.
 */

void
bs_status(hdr, peer)
header *	hdr;	/* request header for this command */
int		peer;	/* Flag for port req. was received from */
{
    bufptr	buf;
    char *	end;
    Res_size	free;
    b_fsize	committed;	/* #bytes cache in use for committed files */
    b_fsize	comm_frag;	/* #bytes fragmentation */
    b_fsize	uncommitted;	/* #bytes cache in use for uncommitted files */
    b_fsize	uncomm_frag;	/* #bytes fragmentation */
    b_fsize	free_slots;	/* # cache slots free */
    b_fsize	comm_rep;       /* #bytes waiting for replication */
    b_fsize	slots_rep;      /* #cache_slots tied in replication */

    errstat	err;
    char	Buf[BS_STATUS_SIZE];

    Stats.b_status++;

    /* Make sure that they have the super capability for this server */
    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_READ, peer)) != STD_OK)
    {
        hdr->h_status = err;
	hdr->h_size = 0;
    }
    else
    {
	buf = Buf;
	end = Buf + BS_STATUS_SIZE;

	/* First the free lists */
	buf = member_status(buf, end) ;
	buf = bprintf(buf, end, "\t\t   In Use       Free\tHoles\n");

	free = num_free_inodes();
	buf = bprintf(buf, end, "Inodes\t\t%9ld  %9ld        N/A\n",
				    - 1, bs_local_inodes_free);
	if ( free != bs_local_inodes_free ) {
		buf = bprintf(buf, end, "Warning, free count is %ld\n",free) ;
	}

	free = a_notused(A_DISKBLOCKS);
	buf = bprintf(buf, end, "Disk Blocks\t%9ld  %9ld  %8ld\n",
		Superblock.s_numblocks - free, free, a_numholes(A_DISKBLOCKS));
	if (a_high_fragmentation(A_DISKBLOCKS))
	   buf = bprintf(buf, end, "\t*** Warning: heavy disk fragmentation\n");

	free = a_notused(A_CACHE_MEM);
	buf = bprintf(buf, end, "Cache Memory\t%9ld  %9ld  %8ld\n\n",
		    Bc_end - Bc_begin - free, free, a_numholes(A_CACHE_MEM));
	cache_usage_stats(&committed, &comm_frag, &comm_rep,
		&uncommitted, &uncomm_frag, &free_slots, &slots_rep);
	buf = bprintf(buf, end,
	    "Committed Files\t\t%8ld bytes cache, %8ld bytes fragmentation\n",
							committed, comm_frag);
	buf = bprintf(buf, end,
	    "Replication\t\t%8ld bytes cache, %8ld slots\n",
						    comm_rep, slots_rep);
	buf = bprintf(buf, end,
	    "Uncommitted Files\t%8ld bytes cache, %8ld bytes fragmentation\n\n",
						    uncommitted, uncomm_frag);

	/* # free cache slots */
	buf = bprintf(buf, end,
	    "Free Cache Slots\t%8ld\n", free_slots);

	/* Free hole structs left for free lists */
	buf = bprintf(buf, end, "Free Hole Structs\t%8ld\n\n",
							    a_numfreeholes());

#if DEBUG_LEVEL<1
	/* Miscellaneous things */
	buf = bprintf(buf, end,
		"cache hits\t%8ld  cache misses\t   %8ld  compactions   %8ld\n",
			Stats.i_cache_hit, Stats.i_cache_miss, Stats.i_compact);
	buf = bprintf(buf, end,
		"read misses\t%8ld  disk reads\t   %8ld  disk writes   %8ld\n",
		    Stats.i_readdata, Stats.i_disk_read, Stats.i_disk_write);
#endif
	buf = bprintf(buf, end,
	    "timed out files\t%8ld  aged files\t   %8ld  std_age       %8ld\n",
			Stats.i_uncomm_tout, Stats.i_aged_files, Stats.std_age);

	/* Function calls */
	buf = bprintf(buf, end,
	   "std_copy(local)\t%8ld  std_copy(remote) %8ld  std_destroy   %8ld\n",
		Stats.std_localcopy, Stats.std_remotecopy, Stats.std_destroy);

	buf = bprintf(buf, end,
		"std_info\t%8ld  std_restrict     %8ld  std_touch\t   %8ld\n",
			Stats.std_info, Stats.std_restrict, Stats.std_touch);

	buf = bprintf(buf, end,
		"b_sync\t\t%8ld  b_status\t   %8ld  b_fsck\t   %8ld\n",
				Stats.b_sync, Stats.b_status, Stats.b_fsck);

	buf = bprintf(buf, end,
		"b_size\t\t%8ld  b_read\t   %8ld  b_create\t   %8ld\n",
				    Stats.b_size, Stats.b_read, Stats.b_create);
#ifdef TODO
	buf = bprintf(buf, end,
		"b_modify\t%8ld  b_insert\t   %8ld  b_delete\t   %8ld\n",
				Stats.b_modify, Stats.b_insert, Stats.b_delete);
#endif	
	buf = bprintf(buf, end,
		"b_disk_compact\t%8ld  moved files\t   %8ld  b_replicate   %8ld\n",
					Stats.b_disk_comp, Stats.b_dcfmoved, Stats.b_replicate);
#if DEBUG_LEVEL>0
	buf = bprintf(buf, end, "max_typed_lock\t%8d  lock overflows   %8d\n",
					bs_max_locks_used, bs_had_ovflw);
#else
	if ( bs_had_ovflw ) {
		buf = bprintf(buf, end, "lock overflows   %8d\n",
					bs_had_ovflw);
	}
#endif

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

/*
 * MS_SYNC
 *
 *	This routine goes through the cache entries and writes all committed
 *	files to disk.  This also occurs in the sweeper but is here for when
 *	the server must be shutdown quickly.
 */

errstat
bs_sync(hdr)
header *	hdr;
{
    errstat	err;

    err = bs_supercap(&hdr->h_priv, (rights_bits) 0,BS_PEER) ;
    if (err != STD_OK) return err ;
    Stats.b_sync++;
    bs_grp_sweep();
    bs_flush_committed();
    bs_block_flush();
    return err;
}

errstat
bs_do_sync()
{
	bs_flush_committed() ;
	bs_block_flush();
	return STD_OK ;
}

errstat
bs_disk_compact(hdr)
header *	hdr;
{
    errstat     err;
 
    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PEER)) != STD_OK)
	return err;

    return bs_do_compact();
}

errstat
bs_fsck(hdr)
header *	hdr;
{
    errstat	err;

    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PEER)) != STD_OK)
	return err;
    
    return bs_do_fsck();
}

errstat
bs_repchk(hdr)
header *	hdr;
{
    errstat     err;
 
    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PEER)) != STD_OK)
	return err;

    return bs_do_repchk();
}


/*
 *  FIND_INODE
 *	This routine finds the inode of the file whose starting block
 *	is "start".  Note that the file size must be non-zero to consume
 *	disk blocks.  It returns a pointer to a write-locked inode.
 *	Since files are immutable, the inode for a committed won't change
 *	unless the file is deleted.  (Multiple diskcompactions in parallel
 *	are excluded.)  Therefore we needn't lock every inode before peeking
 *	at it.
 */

static Inodenum
find_inode(start)
register disk_addr	start;
{
    Inodenum	i;
    Inode *	ip;
    disk_addr	addr;
    disk_addr	size;
    disk_addr	sizechk;

    for (ip = Inode_table+1, i = 1; i < Superblock.s_numinodes; ip++, i++)
    {
	if ( !(ip->i_flags&I_PRESENT) )
	    continue;
	addr = ip->i_dskaddr;
	DEC_DISK_ADDR_BE(&addr);
	size = ip->i_size;
	DEC_DISK_ADDR_BE(&size);
	if (addr == start && size != 0) /* empty files may have bogus addrs */
	{
	    lock_inode(i, L_WRITE);
	    sizechk = ip->i_size;
	    DEC_DISK_ADDR_BE(&sizechk);
	    if (sizechk == size)
	    {
		return i;
	    }
	}
    }
    return 0;
}



/*
 * BS_DISK_COMPACT
 *
 *	This routine is callable only with the super capability.  It goes
 *	through the virtual disk and eliminates fragmentation by moving
 *	all the files down to fill the holes, until there is only one hole.
 *	This takes a lot of time so system administrators shouldn't do it
 *	too much.  Running it at night is best.
 *
 *	The algorithm is as follows:
 *	while (#holes > 1)
 *		find the first hole (free diskblocks) after the last one.
 *		find inode of the file that comes after the hole & lock it
 *		if it is bigger than 1 Meg leave it be
 *		allocate enough blocks in the hole to hold the file to be moved
 *		allocate some cache memory for the file
 *		- if insufficient then the file server is probably busy
 *		  so we stop.
 *		copy the file into the cache memory
 *		adjust the disk address in the inode for the file
 *		write the file to beginning of hole and then write the inode
 *		free the unused disk blocks.
 *	Note that holes will be merged automatically because freeing a piece
 *	of memory adjacent to a hole will merge the two.
 */

#define	BS_ONE_MEG ((1024*1024) >> Superblock.s_blksize)

errstat
bs_do_compact()
{
    static int	compacting; /* to prevent more than one active compactor */

    int		n;
    errstat     err;
    Cache_ent *	cp;
    Res_addr	beginaddr;	/* starting block of a hole */
    Res_size	holesize;	/* size of the hole in blocks */
    Res_addr	faddr;		/* disk address of file blocks to free */
    Res_size	falloc;		/* # blocks to allocate/free from hole */
    Res_size	filesize;	/* size of the file in blocks */
    Inodenum	inode;
 
    DISABLE_PREEMPTION;
    if (compacting)
	return STD_NOTNOW;
    compacting = 1;
    ENABLE_PREEMPTION;

    Stats.b_disk_comp++;
    Stats.b_dcfmoved = 0;

    beginaddr = 0;
    while (a_numholes(A_DISKBLOCKS) > 1)
    {
	threadswitch();
	DISABLE_PREEMPTION;

	/* Find the first free disk blocks (a hole) */
	n = com_firsthole(A_DISKBLOCKS, &beginaddr, &holesize, beginaddr);
	if (n < 0)
	{
	    ENABLE_PREEMPTION;
	    compacting = 0;
	    dbmessage(1, ("bs_disk_compact: com_firsthole failed at addr %x\n",
				beginaddr));
	    return STD_SYSERR;
	}
	if (n == 0) /* no more holes after beginaddr */
	{
	    ENABLE_PREEMPTION;
	    dbmessage(1, ("bs_disk_compact: no more holes\n"));
	    break;
	}

	if (beginaddr + holesize >= Superblock.s_numblocks)
	{
	    ENABLE_PREEMPTION;
	    dbmessage(1, ("bs_disk_compact: last hole at end of disk\n"));
	    break;
	}

	if (holesize > BS_ONE_MEG) /* a hole of about 1Meg */
	{
	    beginaddr++; /* skip to the next hole */
	    ENABLE_PREEMPTION;
	    dbmessage(1, ("bs_disk_compact: skipping big hole %x\n", holesize));
	    continue;
	}

	/* Find and lock inode for the file that begins after this hole */
	if ((inode = find_inode((disk_addr)(beginaddr + holesize))) == 0)
	{
	    ENABLE_PREEMPTION;
	    compacting = 0;
	    dbmessage(1,
	     ("bs_disk_compact: find_hole couldn't find inode after addr %x\n",
			beginaddr + holesize));
	    /* There are no more files behind this hole
	     * Someone must have deleted the file while we were busy.
	     */
	    return STD_SYSERR;
	}

	/* Calculate file size in blocks */
	filesize = Inode_table[inode].i_size;
	DEC_DISK_ADDR_BE(&filesize);
	filesize = NUMBLKS(filesize);
	assert(filesize != 0);

	/*
	 * Now we have to work out how which of the diskblocks are to be
	 * allocated and which blocks will later freed.
	 * Case 1: The hole was smaller than the original file - then we
	 *         free holesize blocks starting at beginaddr+filesize
	 * Case 2: The hole was >= original file size - free original blocks
	 *         of the file.
	 *	In both cases falloc blocks must be freed since that is the
	 *	number of extra blocks allocated.
	 */
	if (filesize > holesize)
	{
	    falloc = holesize;
	    faddr = beginaddr + filesize;
	}
	else
	{
	    falloc = filesize;
	    faddr = Inode_table[inode].i_dskaddr;
	    DEC_DISK_ADDR_BE(&faddr);
	}

	assert(falloc != 0);
	
	/* Allocate enough of the hole to move our file into. */
	if (a_used(A_DISKBLOCKS, beginaddr, falloc) < 0)
	{
	    /* Somebody already allocated a bit of our hole. Sniff */
	    unlock_inode(inode);
	    ENABLE_PREEMPTION;
	    message("bs_disk_compact: preempted hole\n");
	    continue; /* try again */
	}

	ENABLE_PREEMPTION;
	    
	if ((cp = find_cache_entry(inode)) == 0 &&
					(cp = alloc_cache_slot(inode)) == 0)
	{
	    unlock_inode(inode);
	    compacting = 0;
	    return STD_NOSPACE;
	}

	/* Load the file into memory */
	if (cp->c_addr == 0 && (err = bs_loadfile(cp,0)) != STD_OK)
	{
	    unlock_inode(inode);
	    compacting = 0;
	    dbmessage(1, ("bs_disk_compact: bs_loadfile returned %s\n",
								err_why(err)));
	    return err;
	}

	/*
	 * Now the file is in core we move its address on disk and write it
	 * out again - first we de-allocate the unused blocks since this
	 * implies some more checking.
	 */
	if (a_free(A_DISKBLOCKS, faddr, falloc) != 0)
	    bpanic("bs_disk_compact: a_free failed - inconsistent free list");
	cp->c_inode.i_dskaddr = beginaddr;
	cp->c_flags |= (WRITEDATA | WRITEINODE);
	writefile(cp);
	writecache(cp,SEND_CANWAIT);
	unlock_inode(inode);

	Stats.b_dcfmoved++; /* track the number of times through this loop! */
    }
    compacting = 0;
    return STD_OK;
}

