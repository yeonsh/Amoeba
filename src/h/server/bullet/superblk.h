/*	@(#)superblk.h	1.2	94/04/06 16:58:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SUPERBLK_H__
#define __SUPERBLK_H__

/*
**	superblk.h
**
**	Bullet server superblock structure and related constants.
**
**	The superblock of the file system describes fundamental attributes
**	of the file system and how big the current inode table is.
**	The bullet server cannot write this block.  It can only read it.
**
**  Author:
**	Greg Sharp  12-01-89
*/

#define	FLSZ	(D_PHYSBLKSZ - 16 - sizeof (Inodenum) - sizeof (capability) - 2)

typedef struct Superblk	Superblk;	/* structure of super block on disk */

struct Superblk
{
    long	s_magic;	/* consistency check number */
    int		s_blksize;	/* log2(size of file blocks in bytes) */
    Inodenum	s_numinodes;	/* total # inodes in the file system */
    disk_addr	s_numblocks;	/* total # blocks in the file system */
    disk_addr	s_inodetop;	/* blk 0 to here are reserverd for inodes */
    capability	s_supercap;	/* bullet server super capability */
    uint16	s_checksum;	/* for sanity checking */
    char	s_filler[FLSZ];	/* Make up a whole block */
};

/* magic number for bullet server partition */
#define	S_MAGIC		0x70BDCE21

#endif /* __SUPERBLK_H__ */
