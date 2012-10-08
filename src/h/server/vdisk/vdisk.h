/*	@(#)vdisk.h	1.3	94/04/06 17:17:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __VDISK_H__
#define __VDISK_H__

/*
** vdisk.h
**
**	The following defines the structures used for management of virtual
**	disks.  NB: The interface supports a user specified virtual block size.
**	The information stored here is extracted from the disk labels for
**	each piece of disk used by Amoeba.  It is stored in the following
**	form for ease of use.
**	Since all these structures exist only in memory they may contain
**	compiler dependent sizes.
**
** Author:
**	Greg Sharp
*/

/*
** the prefix of the name of the directory entry for the virtual disk server
** in the processor directory.  A :nn suffix is added by dirnappend.
** for boot partition vdisks we have a special name to identify them.
*/
#define	VDISK_SVR_NAME		"vdisk"
#define	BPDISK_SVR_NAME		"bootp"
#define	FLDISK_SVR_NAME		"floppy"

/* largest and smallest acceptable value for log2(virtual block size) */
#define MAX_VBLK_SHIFT          31
#define MIN_VBLK_SHIFT          D_PHYS_SHIFT

/* Maximum number of partitions per virtual disk */
#define MAX_PARTNS_VDISK	8

typedef struct vpart		vpart;
typedef struct vdisktab		vdisktab;


/*
** Virtual Disk Partition Table Structure.
**
** i_physpart:	Pointer to part_list struct of the physical partition on which
**		the sub-partition is found.
** i_part:	The number of the partition (on the physical disk) from which
**		this piece of virtual disk is taken.
** i_firstblk:	Number of the first block in this partition available for use
**		by clients.
** i_numblks:	Number of blocks, starting from i_firstblk, available for use
**		by clients.
*/

struct vpart
{
    part_list *	i_physpart;
    int16	i_part;
    disk_addr	i_firstblk;
    disk_addr	i_numblks;
};


/*
** Virtual Disk Structure.
**
** v_cap:	Capability for this virtual disk.
** v_numblks:	Total number of "physical" blocks available on virtual disk.
** v_many:	Number of entries in the v_parts array.
** v_parts:	Array of in-core partition descriptions of the
**		partitions that comprise the virtual disk.
** v_next:	The virtual disks have to be temporarily kept in a linked list
**		while we build them up.  This is the link pointer.
*/

struct vdisktab
{
    capability	v_cap;
    disk_addr	v_numblks;
    int32	v_many;
    vpart	v_parts[MAX_PARTNS_VDISK];
    vdisktab *	v_next;
};

#endif /* __VDISK_H__ */
