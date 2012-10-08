/*	@(#)diskdep.c	1.9	96/02/27 13:50:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * diskdep.c
 *
 * This file contains the routines dealing with the disks partition
 * table. The partition table is stored in the master boot program
 * which is located at the first sector of the disk.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <string.h>
#include <disk/disk.h>
#include <vdisk/disklabel.h>
#include <vdisk/disktype.h>
#include <pclabel.h>

uint16  checksum _ARGS(( uint16 * data ));

/*
 * This routine reads the master boot block and extracts the
 * partition information from it. This partition table, and
 * the geometry information found in BIOS is used to set up
 * an Amoeba label.
 */
int
host_getlabel(dk_read, devaddr, unit, label, am_label)
    errstat (*dk_read)();		/* disk read routine */
    long devaddr;			/* don't care yet */
    int unit;				/* unit to use */
    char *label;			/* optional return of host label */
    disklabel *am_label;		/* Amoeba disk label */
{
    register struct pcpartition *pe;
    char pclabel[PCL_BOOTSIZE];
    register int i;
    errstat win_geometry();

    /* read partition table */
    if ((*dk_read)(devaddr, unit, 0, 1, pclabel) != STD_OK) {
	printf("host_getlabel: cannot read partition table\n");
	return 1;
    }

    /*
     * Fill the Amoeba label with null geometry for now. It is all filled
     * in later by the first call to disk_getgeometry if it is needed.
     */
    (void) memset((_VOIDSTAR) &am_label->d_geom, 0, sizeof (geometry));

    for (i = 0; i < MAX_PARTNS_PDISK; i++)
	am_label->d_ptab[i].p_firstblk = am_label->d_ptab[i].p_numblks = 0;

    /* check it is a valid partition */
    if (!PCL_MAGIC(pclabel)) {
	printf("WARNING: invalid partition table (%02x%02x): controller at %lx, unit %d.\n",
	    pclabel[BOOT_MAGIC_OFFSET_0]&0xFF, pclabel[BOOT_MAGIC_OFFSET_1]&0xFF,
	    devaddr, unit);
    } else {
	/* fill amoeba label with partition information */
	for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++) {
	    am_label->d_ptab[i].p_firstblk = pe->pp_first;
	    am_label->d_ptab[i].p_numblks = pe->pp_size;
	    if (pe->pp_sysind == PCP_AMOEBA) {
		am_label->d_ptab[i].p_firstblk += AMOEBA_BOOT_SIZE;
		am_label->d_ptab[i].p_numblks -= AMOEBA_BOOT_SIZE;
	    }
	}
    }

    am_label->d_magic = AM_DISK_MAGIC;
    am_label->d_chksum = 0;
    am_label->d_chksum = checksum((uint16 *) am_label);

    /* if they wanted the physical label, give it to them */
    if (label)
	(void) memmove((_VOIDSTAR) label, (_VOIDSTAR) pclabel, PCL_BOOTSIZE);
    return 0;
}

/*
 * Check whether the disk block is a proper label.
 */
int
host_label(h_label)
    char *h_label;
{
    return PCL_MAGIC(h_label);
}

/*
 * This function returns the number of sectors used on the
 * disk to store the primary boot block.
 * We'll set aside AMOEBA_BOOT_SIZE sectors for the Amoeba
 * boot program.
 */
/* ARGSUSED */
disk_addr
host_partn_offset(amlabel)
    disklabel *amlabel;
{
    return 0;
}
