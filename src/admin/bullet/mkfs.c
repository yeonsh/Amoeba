/*	@(#)mkfs.c	1.7	96/02/27 10:12:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	BULLET SERVER MAKE FILE SYSTEM (B_MKFS)
**
**	This routine returns the error status of any operation that failed
**	or STD_OK if all went well.  It simply writes a bullet server
**	superblock on block 0 of the virtual disk specified by the capability.
**	There are two parameters:
**	  disk block size:	Specified as the log2 of the size in bytes.
**				Default is physical block size.
**	  number of inodes:	Default is #disk blocks / 10.
**
**	The program finds out the disk size itself and works the rest out.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "byteorder.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "module/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"

#include "stdlib.h"
#include "stdio.h"

static	Superblk	Super;

errstat
b_mkfs(cap, l2blksz, numinodes)
capability *	cap;
int		l2blksz;
int		numinodes;
{
    uint16	checksum();

    errstat		error;
    disk_addr		numblocks;
    disk_addr		num_halfK_blks;
    disk_addr		inodeblocks;	/* number of blocks for inodes */
    unsigned int	blksz;		/* size of disk block in bytes */
    disk_addr		i;
    char *		nullblock;

/* if l2blksz is unspecified the default is physical block size */
    if (l2blksz == 0)
	l2blksz = D_PHYS_SHIFT;

    if (l2blksz < D_PHYS_SHIFT || l2blksz > 14)
    {
	fprintf(stderr, "Invalid block size: %d\n", l2blksz);
	return STD_ARGBAD;
    }

/* get size of the disk */
    if ((error = disk_size(cap, l2blksz, &numblocks)) != STD_OK)
    {
	fprintf(stderr, "Cannot get size of disk\n");
	return error;
    }

/*
** if # inodes isn't specified then
**	# physical blocks in system / # physical blocks in average sized file
*/
    if (numinodes == 0)
    {
	num_halfK_blks = numblocks << (l2blksz - D_PHYS_SHIFT);
	numinodes = num_halfK_blks / (AVERAGE_FILE_SIZE >> D_PHYS_SHIFT);
	/* If file system is < 250 Mb we probably need more inodes */
	if (num_halfK_blks < 500000)
	    numinodes = (numinodes * 3) / 2;
    }

    
    if (numinodes <= 1)
    {
	fprintf(stderr, "Insufficient inodes: %d\n", numinodes - 1);
	return STD_ARGBAD;
    }

/*
** First we find out how many disk blocks are needed to hold the inodes
** and then we work out the maximum number of inodes that fit in that many
** blocks.
*/
    blksz = 1 << l2blksz;
    inodeblocks = sizeof (Inode) * numinodes;
    inodeblocks = (inodeblocks + blksz - 1) / blksz;
    numinodes = inodeblocks * blksz / sizeof (Inode);

/* fill in the superblock */
    Super.s_magic = S_MAGIC;
    Super.s_blksize = l2blksz;
    Super.s_numinodes = numinodes;
    Super.s_numblocks = numblocks;
    Super.s_inodetop = inodeblocks;
    uniqport(&Super.s_supercap.cap_port);
    uniqport(&Super.s_supercap.cap_priv.prv_random);
    if (prv_encode(&Super.s_supercap.cap_priv, (objnum) 0, PRV_ALL_RIGHTS,
				    &Super.s_supercap.cap_priv.prv_random) < 0)
	return STD_CAPBAD;

    if (Super.s_inodetop > Super.s_numblocks)
    {
	fprintf(stderr, "Disk space for inodes > size of the disk\n");
	return STD_ARGBAD;
    }

/* warn the user about what they are getting */
    printf("Blocksize = %ld bytes\n", (1 << l2blksz));
    printf("Sizeof disk = %ld blocks\n", numblocks);
    printf("Number of inodes = %ld\n", numinodes);
    printf("No. blocks for inodes = %ld\n", Super.s_inodetop);

/*
** Zero the blocks for the inode before writing the superblock,
** since the existence of a valid superblock implies a valid file system
** and the inodes must be *successfully* zeroed before that is made.
*/
/* allocate a block of zeroes the size of a diskblock */
    if ((nullblock = (char *) calloc(1, blksz)) == 0)
	return STD_NOMEM;

    for (i = 1; i <= Super.s_inodetop; i++)
    {
	error = disk_write(cap, l2blksz, i, (disk_addr) 1, nullblock);
	if (error != STD_OK)
	    return error;
    }

/* encode to big endian */
    enc_l_be(&Super.s_magic);
    enc_l_be(&Super.s_blksize);
    ENC_INODENUM_BE(&Super.s_numinodes);
    ENC_DISK_ADDR_BE(&Super.s_numblocks);
    ENC_DISK_ADDR_BE(&Super.s_inodetop);

/* put in checksum */
    Super.s_checksum = 0;
    Super.s_checksum = checksum((uint16 *) &Super);

/* write out the superblock */
    return disk_write(cap, D_PHYS_SHIFT, (disk_addr) 0,
					    (disk_addr) 1, (char *) &Super);
}


/*
 * check_disk
 *	Returns STD_OK if the disk specified by disk_cap does not contain
 *	a bullet file system.  Otherwise it returns an error code.
 */
errstat
check_disk(disk_cap)
capability *	disk_cap;
{
    uint16      checksum();

    uint16      sum;
    errstat 	err;

    err = disk_read(disk_cap, D_PHYS_SHIFT, (disk_addr) 0, (disk_addr) 1,
							    (char *) &Super);
    if (err != STD_OK)
	return err;

    /* calculate superblock checksum */
    sum = checksum((uint16 *) &Super);

    /* decode the data from big endian to current architecture */
    dec_l_be(&Super.s_magic);
    if (Super.s_magic == S_MAGIC && sum == 0)
        return STD_EXISTS;
    return STD_OK;
}
