/*	@(#)dsktab.h	1.3	94/04/06 12:04:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DSKTAB_H__
#define __DSKTAB_H__

/*
 *	The dsktype struct holds the default geometry information for
 *	supported disks.  At present it is fairly simple since we only
 *	know about a few disk controllers.
 *
 *	The dsktab struct is used to hold the details and physical label of
 *	the disks found on the system.  The amoeba information field must
 *	be filled in by a host dependent routine.
 */

#define	NO_LABEL	-1

#define	DNAMELEN	12

/*
 * diskcap	: Capability for this disk.
 * name		: The name of the disk in the kernel directory.
 * size		: Size of the disk in 1/2K blocks.
 * labelled	: The value of this variable = index of Host_p array
 *		  of the label.  -1 = no identifiable label.
 * next		: This structure is used as a linked list.
 * plabel	: Copy of the physical label, if there is one.
 * amlabel	: Details of the partition table in a data structure that
 *		  Amoeba knows.  It must be filled in by the host OS dependent
 *		  routines.
 */

typedef struct dsktab	dsktab;
struct dsktab
{
    capability	diskcap;
    char	name[DNAMELEN];
    disk_addr	size;
    int		labelled;
    dsktab *	next;
    char	plabel[D_PHYSBLKSZ];
    disklabel	amlabel;
};


/*
 *  Part_list is the basis for a list of partitions for use by amoeba
 *
 * diskcap	: Capability for this disk.
 * name		: The name of the disk in the kernel directory.
 * partnum	: Number of this partition in disk's partition table.
 * next		: This structure is used as a linked list.
 * labelled	: 1 if a valid vdisk label exists on the partn, else 0
 * labelblock	: The disk address where the partition label belongs.
 * firstblock	: The first block on the partition.
 * size		: Size of the partition in 1/2K blocks.
 * label	: Details of the partition table in a data structure that
 *		  Amoeba knows.  It must be filled in by the host OS dependent
 *		  routines.
 */

typedef struct part_list	part_list;
struct part_list
{
    capability	diskcap;
    char	name[DNAMELEN];
    int		partnum;
    int		labelled;
    part_list *	next;
    disk_addr	labelblock;
    disk_addr	firstblock;
    disk_addr	size;
    disklabel	label;
};


/*
 * name		: Human readable string describing the brand and type of disk.
 * geom		: Geometry of the physical disk
 */

typedef struct dsktype	dsktype;
struct dsktype
{
    char	name[30];
    geometry	geom;
};

#endif /* __DSKTAB_H__ */
