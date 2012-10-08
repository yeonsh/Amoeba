/*	@(#)disktype.h	1.2	94/04/06 17:17:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DISKTYPE_H__
#define __DISKTYPE_H__

/*
**	The dsktype struct holds the default geometry information for
**	supported disks.  At present it is fairly simple since we only
**	know about a few disk controllers.
**
**	The dsktab struct is used to hold the details and physical label of
**	the disks found on the system.  The amoeba information field must
**	be filled in by a host dependent routine.
*/

#define	AM_LABEL	2
#define	HOST_LABEL	1
#define	NO_LABEL	0

/* for partn_usage field */
#define	HOST_PARTN	0
#define	AMOEBA_PARTN	1

/*
** type		: Index into table of known controller types (in diskconf.c).
** address	: Must be non-zero.  Parameter to device driver to specify the
**		  address of the disk controller to be used.
** unit		: Specifies the unit number (on the controller) of the disk.
*/

typedef struct	contlr	contlr;
struct	contlr
{
    int		type;
    long	address;
    int		unit;
};


/*
** ctlr		: Details of the controller for this disk.
** labelled	: 2 if the disk has an Amoeba label,
**		  1 if the disk has a host label
**		  and 0 otherwise.
** next		: This structure is used as a linked list.
** plabel	: Copy of the physical label, if there is one.
** amlabel	: Details of the partition table in a data structure that
**		  Amoeba knows.  It must be filled in by the host OS dependent
**		  routines.
** partn_usage	: Says if partition is for use by amoeba or not.
*/

typedef struct dsktab	dsktab;
struct dsktab
{
    contlr	ctlr;
    int		labelled;
    dsktab *	next;
    char	plabel[D_PHYSBLKSZ];
    disklabel	amlabel;
    int		partn_usage[MAX_PARTNS_PDISK];
};


/*
**  Part_list is the basis for a list of partitions for use by amoeba
**
** ctlr		: Details of the controller for the disk of this partition.
** next		: This structure is used as a linked list.
** labelblock	: The disk address where the partition label belongs.
** label	: Details of the partition table in a data structure that
**		  Amoeba knows.  It must be filled in by the host OS dependent
**		  routines.
*/

typedef struct part_list	part_list;
struct part_list
{
    contlr	ctlr;
    part_list *	next;
    disk_addr	labelblock;
    disklabel	label;
};


/*
** name		: Human readable string describing the brand and type of disk.
** geom		: Geometry of the physical disk
*/

typedef struct dsktype	dsktype;
struct dsktype
{
    char	name[30];
    geometry	geom;
};

#endif /* __DISKTYPE_H__ */
