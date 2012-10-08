/*	@(#)pclabel.h	1.3	94/04/06 16:45:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * pclabel.h
 *
 * A PC hard disk may contain four partitions of which the first starts at
 * sector 2.  The first sector holds the stage one bootstrap loader, which
 * also contains the actual partition table.
 * Every partition belongs to an operating system, which system index
 * is stored in the particular table entry. Within a partition an operating
 * system is free to do what it pleases, except for the notion that the first
 * sector of the partition contains a stage two bootstrap loader. 
 * For an Amoeba partition the convention exists that the first track is
 * reserved for the stage two bootstrap loader, the rest of the partition
 * is sub-partitioned into one or more Amoeba virtual disks.
 *
 * NOTE: all the values in these structures are in writen out in the
 *	 i80386 native byte order, that is little endian.
 */

/* size of boot programs in blocks (512 bytes) */
#define	MASTER_BOOT_SIZE	1	/* size of primary boot */
#define	AMOEBA_BOOT_SIZE	17	/* size of amoeba's secondary boot */

/* every pc boot block is marked by a magic number */
#define	BOOT_MAGIC_BYTE_0	0x55	/* magic byte 0 for boot blocks */
#define	BOOT_MAGIC_BYTE_1	0xAA	/* magic byte 1 for boot blocks */
#define	BOOT_MAGIC_OFFSET_0	(512-2)	/* offset for magic byte 0 */
#define	BOOT_MAGIC_OFFSET_1	(512-1)	/* offset for magic byte 1 */

/* PC label constants */
#define	PCL_NPART	4	/* # of partition entries */
#define	PCL_PARKCYLS	1	/* # of cylces reserved for parking zone */
#define	PCL_BOOTSIZE	512	/* boot block size */
#define	PCL_LG2BOOTSIZE	9	/* log2(PCL_BOOTSIZE) */
#define	PCL_PCP_OFFSET	446	/* offset of pc partition table */

/* useful macros */
#define	PCL_MAGIC(b)	((b[BOOT_MAGIC_OFFSET_0]&0xFF) == BOOT_MAGIC_BYTE_0 && \
			 (b[BOOT_MAGIC_OFFSET_1]&0xFF) == BOOT_MAGIC_BYTE_1)
#define	PCL_PCP(b)	((struct pcpartition *) &b[PCL_PCP_OFFSET])

/* boot indicator values */
#define	PCP_ACTIVE	0x80	/* active (bootable) partition */
#define	PCP_PASSIVE	0x00	/* passive (non bootable) partition */

/* system identifiers */
#define	PCP_NOSYS	0x00	/* no system index */
#define	PCP_AMOEBA	0x93	/* Amoeba partition */
#define	PCP_BADBLK	0x94	/* Amoeba bad block partition */

/*
 * Partition table entry
 */
struct pcpartition {
    uint8  pp_bootind;		/* boot indicator PART_PASSIVE /PART_ACTIVE */
    uint8  pp_start_head;	/* head value for first sector */
    uint8  pp_start_sec;	/* sector value + cyl bits for first sector */
    uint8  pp_start_cyl;	/* track value for first sector */
    uint8  pp_sysind;		/* system indicator */
    uint8  pp_last_head;	/* head value for last sector */
    uint8  pp_last_sec;		/* sector value + cyl bits for last sector */
    uint8  pp_last_cyl;		/* track value for last sector */
    uint32 pp_first;		/* logical first sector */
    uint32 pp_size;		/* size of partion in sectors */
};

