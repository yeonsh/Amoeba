/*	@(#)inode.h	1.1	96/02/27 10:35:40 */
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

#define DEC_VERSION_BE(x)	dec_l_be(x)
#define ENC_VERSION_BE(x)	enc_l_be(x)

/* Version numbers to use are in the interval 1..MAX_VERSION.
 * MAX_VERSION+VERSION_WINDOW <= 2^31, for ease of calculation
 */
#define UNKNOWN_VERSION		0
#define MAX_VERSION		2147400000
#define VERSION_WINDOW		65536

/*
** An inode struct is a descriptor for a file in the file system.
** A table of inodes is kept at the start of the file system. It tells
** what files are in the system and where to find them on disk.
**
** i_dskaddr:	This is the disk address.  If the file is in the cache table
**		then the memory address is stored there.
**		If the file exists, but is not present this field contains
**		a bit map of server id's on which the file's contents are
**		stored.
** i_size:	This is the size of the file in bytes.
** i_random:	This is the original random number for the capability for
**		the file.
** i_owner:	This entry contains the id of a server that has control
**		over it. ID_UNKNOWN is a valid value. If the storage
**		server's own id is here this inode is free to be
**		(de)allocated by this server. The group bullet server
**		does not allow owner numbers>8 at the moment.
** i_flags:	Used for a diversity of status operations. 
**		Replication status: This allows
**		replication to continue after a server crash.
**		Destroy status: The destroy flag is set when an inode is
**		destroyed on the current storage server and cleared when
**		all replica's have disappeared.
**		Inode allocation status: and
**		Inode locking is not valid on disk, but only in core.
**		Until a file is committed the inode can be allocated but not
**		a have valid inode on disk.
**		The bullet server allows use of only the first 16 bits.
** i_version:	The version number of the file. Round robin between 1
**		and MAX_VERSION-1. 0 is special and means `unknown version'.
*/

struct Inode
{
    char		i_flags;	/* status information used */
    unsigned char	i_l_age;	/* Local age */
    unsigned char	i_owner;	/* the owner, used */
    char		i_g_age;	/* Global age */
    uint32		i_version;	/* Version number */
    disk_addr		i_dskaddr;	/* disk address of the file */
    b_fsize		i_size;		/* size of file in bytes */
    port		i_random;	/* raw cap for the file */
    char		i_filler[10];	/* To make a total of 32 */
};

/* The next definition is identical to `struct Inode' except
 * that it does not touch the i_filler, in which the lifetime's are hidden.
 * This structure is used inside the cache.
 */
/* When copying the inode back from cache it leave the values in the
 * in core inod etable for ownership, ttl's, version, random untouched 
 */
struct Inode_cache
{
    char		i_flags;	/* status information used */
    unsigned char	i_filler1;	/* Age is never touched inside cache */
    unsigned char	i_owner;	/* the owner, used */
    unsigned char	i_filler2;	/* Age is never touched inside cache */
    uint32		i_version;	/* Version number */
    disk_addr		i_dskaddr;	/* disk address of the file */
    b_fsize		i_size;		/* size of file in bytes */
    port		i_random;	/* raw cap for the file */
};

/* valid values for i_flags */
#define	I_ALLOCED	0x01	/* If inode is allocated, in-core only */
#define	I_REPTO		0x02	/* Used as flag in send_inodes */
#define I_BUSY		0x04	/* If busy, see below */
#define I_DESTROY	0x10	/* If being destroyed */
#define I_PRESENT	0x20	/* If on disk of storage server */
#define I_INTENT	0x40	/* If control over inode changes hands */
#define I_INUSE		0x80	/* If inode is in use for committed file */

/* The BUSY flag is used in combination with others.
   For committed files in core this means that replication for the file
   is still not complete.
   On disk this flag has no meaning.
 */
/* In combination with a commited inode I_INTENT means: the incore
   inode is correct, but the inode is not on disk yet and the disk
   addres is still zero.
 */

/*
 * For a sanity check at start up we test the inode size to make sure it
 * what we expected.
 */
#define	INODE_SIZE	32

/*
 * maximum value for i_lifetime. It specifies time before the file is
 * garbage collected (time unit is determined by system administrator)
 */
#define	MAX_LIFETIME	24

/* The type used for bitmaps containing peer numbers.
 * The code contains a sanity check a start-up.
 */
typedef BS_PEER_MAP_TYPE peer_bits ;

/* Constants for make_cache_entry/validate_cap/lock_inode */
#define	L_READ		0
#define	L_WRITE		0	/* Identical for the moment */
#define L_NONE		2

#endif /* __INODE_H__ */
