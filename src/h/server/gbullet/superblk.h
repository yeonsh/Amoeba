/*	@(#)superblk.h	1.1	96/02/27 10:35:41 */
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
**
**  Author:
**	Greg Sharp  12-01-89
**	Ed Keizer   93/04/22	Group server version
*/

#define	S_MAXMEMBER	8		/* Maximum # members in superblock */
#define	FLSZ	(D_PHYSBLKSZ -4 -8*sizeof(uint16) - sizeof (Inodenum) \
	- 4*sizeof (capability) - sizeof (port) \
	- S_MAXMEMBER*(sizeof(uint16)+sizeof (capability)) \
	- 2*sizeof(disk_addr) )

typedef struct Superblk	Superblk;	/* structure of super block on disk */

struct Superblk
{
    long	s_magic;	/* consistency check number */
    uint16	s_version;	/* Superblock & Inode table version */
    uint16	s_maxmember;	/* Maximum number of servers in group */
    uint16	s_member;	/* Storage server id */
    uint16	s_def_repl;	/* Default number of replicas */
    uint16	s_blksize;	/* log2(size of file blocks in bytes) */
    uint16	s_flags;	/* Various flags, see below */
    uint16	s_checksum;	/* for sanity checking */
    uint16	s_seqno ;	/* Sequence number of group structure */
    Inodenum	s_numinodes;	/* total # inodes in the file system */
    disk_addr	s_numblocks;	/* total # blocks in the file system */
    disk_addr	s_inodetop;	/* blk 0 to here are reserved for inodes */
    port	s_fileport;	/* The port used for file access */
    capability	s_groupcap;	/* Group communication capability */
    capability	s_supercap;	/* file server super get-capability */
    capability	s_membercap;	/* Storage server get-capability */
    capability	s_logcap;	/* Logging put-capability */
    struct {
	capability ms_cap;	/* Put-capability of member */
	uint16	   ms_status;	/* Status of member */
    } s_memstatus[S_MAXMEMBER] ;  /* This entry must be last */
    char	s_filler[FLSZ];	/* Make up a whole block */
};

/* magic number for bullet server partition */
#define	S_MAGIC		0x01ABBB01

/* magic number of previous bullet server */
#define S_OLDMAGIC	0x70BDCE21

/* This version's number */
#define S_VERSION		1

/* The id used for storage server's unaware of their id */
#define S_UNKNOWN_ID		255

/* Values for ms_status */
/* Do no change the order, it is used */
#define MS_NONE		0	/* non-existent, the default,
				   must be zero! */
#define MS_NEW		1	/* allocated, but not yet seen */
#define MS_ACTIVE	2	/* A full member */
#define MS_PASSIVE	3	/* Do not create new files on this one */

/* Various flags for s_flags */
#define S_SAFE_REP	1	/* Always make replica when safety is on */

#endif /* __SUPERBLK_H__ */
