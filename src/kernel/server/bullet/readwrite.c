/*	@(#)readwrite.c	1.9	96/02/27 14:13:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * READWRITE.C
 *
 *	This file contains the routines for validating the superblock and
 *	reading the inode table into core and unpacking them and the routines
 *	to pack an inode and write it to disk.
 *
 *  Author: 
 *      Greg Sharp  12-01-89
 */

#include "amoeba.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */
#include "cmdreg.h"
#include "stderr.h"
#include "byteorder.h"

#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "cache.h"
#include "alloc.h"
#include "bs_protos.h"
#include "assert.h"

/* In core copy of the super block */
extern Superblk	Superblock;

/* Pointer to in core copy of the inode table */
extern Inode *	Inode_table;

/* Size of a disk block in bytes */
extern b_fsize	Blksizemin1;


/*
 * VALID_SUPERBLK
 *
 *	This routine looks at Superblock to it to see if it is a valid
 *	superblock.   If it is it returns 1. Otherwise it returns 0.
 *	The superblock is stored in big-endian format on the disk.  This
 *	routine unpacks it to the "endian" of the host architecture. 
 * 
 */

int
valid_superblk()
{
    uint16	checksum();

    uint16	sum;

    /* Calculate superblock checksum */
    sum = checksum((uint16 *) &Superblock);

    /* Decode the data from big endian to current architecture */
    dec_l_be(&Superblock.s_magic);
    if (Superblock.s_magic != S_MAGIC || sum != 0)
	return 0;
    dec_l_be(&Superblock.s_blksize);
    DEC_INODENUM_BE(&Superblock.s_numinodes);
    DEC_DISK_ADDR_BE(&Superblock.s_numblocks);
    DEC_DISK_ADDR_BE(&Superblock.s_inodetop);
    Blksizemin1 = (1 << Superblock.s_blksize) - 1;
    return 1;
}


/*
 * READ_INODETAB
 *
 *	This routine reads the inode table from disk.  The inode table is
 *	stored in big-endian format on the disk.  It remains in BE format
 *	in core and is only decoded when the inode is cached.
 */

void
read_inodetab(numblks)
disk_addr	numblks;
{
    /*
     * Read inode table (starting at disk block 1) into top of buffer cache
     * memory since disk can DMA to there.
    */
    if (bs_disk_read((disk_addr) 1, (bufptr) Inode_table, numblks) != STD_OK)
	bpanic("read_inodetab: bs_disk_read failed");
}


/*
 * WRITECACHE
 *
 *	This routine encodes an inode into big endian format and
 *	writes it away to disk.  Note, it is usually not necessary to write the
 *	entire inode table since the on-disk copy is always kept up to date.
 */

void
writecache(cp)
Cache_ent *	cp;
{
    Inode *	ip;

    assert(inode_locked_by_me(cp->c_innum) == 2);
    assert(!(cp->c_inode.i_flags & I_INTENT));
    ip = Inode_table + cp->c_innum;
    *ip = cp->c_inode;
    ENC_DISK_ADDR_BE(&ip->i_dskaddr);
    ENC_FILE_SIZE_BE(&ip->i_size);
    if (!(cp->c_flags & NOTCOMMITTED))	/* only write committed inodes! */
    {
	writeinode(ip);
	cp->c_flags &= ~WRITEINODE;
    }
}


/*
 * WRITEINODE
 *
 *	This routine figures out which block of memory the in core inode
 *	is in and writes that block to the appropriate place on the disk.
 *	Note that inodes occupy blocks 1 to Superblock.s_inodetop on the
 *	disk, so we need to add 1 to the block number.
 */

void
writeinode(ip)
Inode *	ip;
{
    bufptr	caddr;	/* pointer to cache memory block holding inode */
    disk_addr	block;	/* diskblock where inode should be written */
    errstat	err;	/* status from bs_disk_write */
    Inodenum	inode;	/* number of inode ip */

    inode = ip - Inode_table;
    assert(inode_locked_by_me(inode) == 2);
    /* Must point to start of block containing inode */
    caddr = (bufptr) (((long) ip) & ~Blksizemin1);
    block = (((inode) * sizeof (Inode)) >> Superblock.s_blksize) + 1;
    if (block < 1 || block > Superblock.s_inodetop)
	bpanic("writeinode: illegal inode %d", inode);

    if ((err = bs_disk_write(caddr, block, (disk_addr) 1)) != STD_OK &&
		(err = bs_disk_write(caddr, block, (disk_addr) 1)) != STD_OK)
	bpanic("writeinode: bs_disk_write failed (%d)", err);
}


/*
 * WRITEFILE
 *
 *	This routine writes the data of a file from the cache to disk.
 */

void
writefile(cp)
Cache_ent *	cp;
{
    disk_addr	nblks;
    disk_addr	daddr;
    errstat	err;

    assert(inode_locked_by_me(cp->c_innum) == 2);
    assert(cp->c_flags & WRITEDATA);

    cp->c_flags &= ~WRITEDATA;

    /* Calculate the number of blocks in the file */
    if ((nblks = NUMBLKS(cp->c_inode.i_size)) == 0)
	return;	/* nothing to write */

    daddr = cp->c_inode.i_dskaddr;

    /* Get the data written */
    if (daddr <= Superblock.s_inodetop)
	bpanic("writefile: trying to write data to inode disk area.");

    /* Check that the allocated diskblocks exist! */
    if (daddr >= Superblock.s_numblocks ||
				    daddr + nblks > Superblock.s_numblocks)
    {
	printf("writefile: trying to write to non-existent disk blocks\n");
	printf("writefile: disk address = %lx, #blks = %lx\n", daddr, nblks);
	printf("writefile: max diskblock = %lx\n", Superblock.s_numblocks);
	bpanic("writefile: internal inconsistency.");
    }

    assert(cp->c_addr);
    /* Try the write again if it fails the first time */
    if ((err = bs_disk_write(cp->c_addr, daddr, nblks)) != STD_OK
		&& (err = bs_disk_write(cp->c_addr, daddr, nblks)) != STD_OK)
    {
	printf("bs_disk_write: addr=%d, nblks=%d, err=%d\n", daddr, nblks, err);
	bpanic("writefile: bs_disk_write failed");
    }
}
