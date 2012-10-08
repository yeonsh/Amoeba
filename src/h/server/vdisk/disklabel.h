/*	@(#)disklabel.h	1.3	94/04/06 17:16:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DISKLABEL_H__
#define __DISKLABEL_H__

/*
** disklabel.h
**
**	Format of the Amoeba disk label.
**
**	The Amoeba disk label resides on cylinder 0, sector 0 of each
**	contiguous piece of disk allocated to Amoeba.
**	It is used for both SMD and SCSI type disks.  Hopefully it will work
**	with other interfaces which appear.
**	Amoeba supports virtual disks.  Virtual disks consist of one or more
**	partitions, not necessarily on the same device.
**	Amoeba can also cohabit disks with other operating systems that
**	partition disks.  If the disk label is that of another operating
**	system, Amoeba will try to recognise it, find the partition table and
**	go through the partitions until it finds one with an Amoeba
**	disk label and then treat that partition of the disk as an Amoeba disk
**	(which will be further divided into Amoeba partitions).
**
**	Disk threads not supporting virtual disks simply ignore the virtual
**	disk information in the partition table.
**
** NB: sizeof (Disk_label) should be the sector size.
**
** Author:
**	Greg Sharp
*/

/* maximum # of Amoeba partitions per physical disk */
#define	MAX_PARTNS_PDISK	8

#define	AM_DISK_MAGIC		0x0A30EBA0	/* Amoeba magic number */

typedef struct part_tab		part_tab;
typedef	struct geometry		geometry;
typedef	struct disklabel	disklabel;


/*
** Partition Table Structure.
**
** p_firstblk:	gives the first physical block number of the partition.
**		For purity of implementation this should be the start of a
**		cyclinder.
** p_numblks:	gives the number of physical blocks allocated to this
**		partition.
** p_piece:	each partition forms a part of exactly one virtual disk.
**		p_piece specifies the number of the part that this partition
**		is.
** p_many:	p_many specifies the total number of parts that make up the
**		virtual disk.
** p_cap	is the capability of the virtual disk to which this partition
**		belongs.
*/
struct part_tab
{
    disk_addr	p_firstblk;
    disk_addr	p_numblks;
    int16	p_piece;
    int16	p_many;
    capability	p_cap;
};


/*
** Disk Geometry Structure.
**
** g_bpt:	Bytes per track.
** g_bps:	Bytes per sector.
** g_numcyl:	Number of data cylinders (!= total cylinders on disk). 	These
**		begin from clynder 0.
** g_altcyl:	Number of cylinders reserved for bad block mapping. These begin
**		after the data cylinders and usually go to the end of the disk.
** g_numhead:	Number of read/write heads.
** g_numsect:	Number of sectors per track.
*/
struct geometry
{
    int16	g_bpt;
    int16	g_bps;
    int16	g_numcyl;
    int16	g_altcyl;
    int16	g_numhead;
    int16	g_numsect;
};


/*
** Disk Label Structure.
**
** d_filler:	Pads the disk label out to physical block size.
** d_disktype:	A string describing the brand and type of the disk.
** d_geom:	The disk geometry parameters are only relevant for SMD
**		drives since scsi disks manage this information internally.
** d_ptab:	Partition table.
** d_magic:	A quick check to make sure that this really is an Amoeba label.
** d_filler2:	To make sure that the struct fills to a 4 byte boundary.
** d_chksum:	This is an xor of all the 16-bit words in the disk label to
**		give some confidence that the label hasn't been corrupted.
*/

/* max # chars describing the type of disk */
#define	TYPELEN		30

/* filler size must pad the struct to D_PHYSBLKSZ bytes. */
/* Must be adjusted for Amoeba 4! */
#define	FILLER_SIZE	(D_PHYSBLKSZ - sizeof (geometry) \
				- MAX_PARTNS_PDISK*sizeof (part_tab) \
				- sizeof (int32) - 2*sizeof (uint16) - TYPELEN)

struct disklabel
{
    char	d_filler[FILLER_SIZE];
    char	d_disktype[TYPELEN];
    geometry	d_geom;
    part_tab	d_ptab[MAX_PARTNS_PDISK];
    int32	d_magic;
    uint16	d_filler2;
    uint16	d_chksum;
};

#endif /* __DISKLABEL_H__ */
