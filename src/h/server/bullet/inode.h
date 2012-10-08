/*	@(#)inode.h	1.6	96/02/27 10:36:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __INODE_H__
#define __INODE_H__

/*
**	INODE.H
**
**	Bullet Server inode structure and related constants
**
**  Author:
**	Greg Sharp 17-01-89
*/

typedef long		Inodenum;	/* index into inode table */
typedef struct Inode	Inode;		/* disk inode table entries */

#define	NULL_INODE	((Inode *) 0)

/*
** decode/encode an inode number from/to host architecture to/from big-endian
** format.  DEC_L_BE and ENC_L_BE are defined in byteorder.h
*/

#define	DEC_INODENUM_BE(x)	dec_l_be(x)
#define	ENC_INODENUM_BE(x)	enc_l_be(x)

/*
** An inode struct is a descriptor for a file in the file system.
** A table of inodes is kept at the start of the file system. It tells
** what files are in the system and where to find them on disk.
**
** i_dskaddr:	This is the disk address.  If the file is in the cache table
**		then the memory address is stored there.  Note that the
**		superblock has address 0 so the inode is unsed if this is 0.
** i_size:	This is the size of the file in bytes.
** i_random:	This is the original random number for the capability for
**		the file.
** i_lifetime:	This is a count used by the garbage collector.  If the count 
** 		reaches zero the file is deleted by the garbage collector.
**		The count is decremented every 24 hours after the start of the
**		bullet server.
** i_flags:	Only the I_INTENT flag is valid on disk.  The other flags
**		are valid only in the cache.  The INTENT flag means that the
**		disk copy of the inode is not yet valid but soon will be.
**		Until a file is committed the inode can be allocated but not
**		a valid inode.  If i_lifetime is set the inode is in use.
**		The inode allocator will correctly set the value in the
**		in-core copy.
*/

struct Inode
{
    disk_addr	i_dskaddr;	/* disk address of the file */
    b_fsize	i_size;		/* size of file in bytes */
    port	i_random;	/* raw cap for the file */
    char	i_lifetime;	/* count, decremented each day until 0 */
    char	i_flags;	/* non-zero if inode has been allocated */
};

/* valid values for i_flags */
#define	I_ALLOCED	0x01	/* If inode is allocated */
#define	I_LOCKED	0x02	/* If inode is locked */
#define	I_INTENT	0x40	/* If inode is not yet valid */

/*
 * For a sanity check at start up we test the inode size to make sure it
 * what we expected.
 */
#define	INODE_SIZE	16

/*
 * maximum value for i_lifetime. It specifies time before the file is
 * garbage collected (time unit is determined by system administrator)
 */
#if !defined(MAX_LIFETIME)
#define	MAX_LIFETIME	24
#endif

/* Constants for make_cache_entry/validate_cap/lock_inode */
#define	L_READ		0
#define	L_WRITE		1
#define L_NONE		2

#endif /* __INODE_H__ */
