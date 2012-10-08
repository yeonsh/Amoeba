/*	@(#)ultpart.h	1.2	94/04/06 16:46:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ULTPART_H__
#define __ULTPART_H__

/* Ultrix (4.3 BSD?) superblock and partition table definitions.
   Only as much is defined here as is needed to get the partition table
   in host_getlabel in diskdep.c. */

#define SB_SIZE		8192		/* Superblock size (bytes) */
#define SB_OFFSET	8192		/* Superblock offset in disk (bytes) */

#define FS_MAGIC	0x00011954	/* File system magic number */
#define FS_MAGOFFSET	0x55c		/* Its byte offset in the superblock */

#define PT_MAGIC	0x00032957	/* Partition table magic number */
#define PT_NPART	8		/* Number of partitions */

/* Partition table lay-out; resides at very end of superblock */
struct pt {
	long pt_magic;			/* Magic number */
	long pt_pad;			/* Not used */
	struct pt_part {
		long pt_size;		/* Size of partition in blocks */
		long pt_start;		/* First block of partition */
	} pt_tab[PT_NPART];
};

#define PT_OFFSET	(SB_SIZE - sizeof(struct pt))

#endif /* __ULTPART_H__ */
