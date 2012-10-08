/*	@(#)dos_dep.c	1.4	96/02/27 10:12:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * DOS Disklabel Dependencies
 *
 *	This file contains the host operating system dependencies for
 *	dealing with DOS disks.  Primarily they are concerned with the disk
 *	label and partitions of DOS.  They read/verify labels and copy the
 *	information about the disk into a struct known by Amoeba.
 *	At the end of the file is a struct containing the pointers to the
 *	routines defined.  This is the only external reference defined in
 *	this file.  All the routines are static.
 *
 * Author: Gregory J. Sharp, June 1992
 *		- poached from Leendert's version for the old disklabel
 */

#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdio.h"
#include "string.h"
#include "module/disk.h"
#include "disk/conf.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "pclabel.h"
#include "host_dep.h"
#include "proto.h"

extern char *		Prog;


static void
decode_pclabel(pclabel)
char *	pclabel;
{
    int i;
    struct pcpartition * pe;
    uint32 first;
    uint32 size;

    for (pe = PCL_PCP(pclabel), i = 0; i < PCL_NPART; pe++, i++)
    {
	/* these things aren't properly aligned and so we add a hack
	 * to make them work
	 */
	memcpy((_VOIDSTAR) &first, (_VOIDSTAR) &pe->pp_first, sizeof (first));
	DEC_INT32_LE(&first);
	memcpy((_VOIDSTAR) &pe->pp_first, (_VOIDSTAR) &first, sizeof (first));

	memcpy((_VOIDSTAR) &size, (_VOIDSTAR) &pe->pp_size, sizeof (size));
	DEC_INT32_LE(&size);
	memcpy((_VOIDSTAR) &pe->pp_size, (_VOIDSTAR) &size, sizeof (size));
    }
}


static void
encode_pclabel(pclabel)
char *	pclabel;
{
    int i;
    struct pcpartition * pe;
    uint32 first;
    uint32 size;

    for (pe = PCL_PCP(pclabel), i = 0; i < PCL_NPART; pe++, i++)
    {
	memcpy((_VOIDSTAR) &first, (_VOIDSTAR) &pe->pp_first, sizeof (first));
	ENC_INT32_LE(&first);
	memcpy((_VOIDSTAR) &pe->pp_first, (_VOIDSTAR) &first, sizeof (first));

	memcpy((_VOIDSTAR) &size, (_VOIDSTAR) &pe->pp_size, sizeof (size));
	ENC_INT32_LE(&size);
	memcpy((_VOIDSTAR) &pe->pp_size, (_VOIDSTAR) &size, sizeof (size));
    }
}


/*ARGSUSED*/
static void
dos_labeldisk(dp, type)
dsktab *	dp;	/* pointer to disk to be labelled */
int		type;	/* -1 if current label is to be modified */
{
    printf("\n\7\7\7Use fdisk to partition the disk\n");
}


static int
dos_getlabel(diskcap, label, am_label)
capability *	diskcap;	/* disk to read */
char *		label;		/* optional return of host label */
disklabel *	am_label;	/* Amoeba disk label */
{
    register struct pcpartition *pe;
    register int i, n;
    char pclabel[PCL_BOOTSIZE];
    errstat err;

    /* Read partition table */
    err = disk_read(diskcap, D_PHYS_SHIFT, (disk_addr) 0, (disk_addr) 1, pclabel);
    if (err != STD_OK)
    {
	fprintf(stderr, "%s: dos_getlabel: disk read failed (%s)\n",
							Prog, err_why(err));
	return -1;
    }

    /* Check it is a valid partition */
    decode_pclabel(pclabel);
    if (!PCL_MAGIC(pclabel))
    {
	encode_pclabel(pclabel);

	/* If they wanted the physical label, give it to them */
	if (label)
	    (void) memmove(label, pclabel, PCL_BOOTSIZE);

	return 1;
    }

    if (label)
	(void) memmove(label, pclabel, PCL_BOOTSIZE);
    
    /*
     * Now we have to build the Amoeba version of the DOS label
     */

    /* We start by getting the geometry */
    err = disk_getgeometry(diskcap, &am_label->d_geom);
    if (err != STD_OK)
    {
	fprintf(stderr, "%s: dos_getlabel: can't get geometry (%s)\n",
							Prog, err_why(err));
	return -1;
    }

    /* Zero out the partition table - it may be bigger than the PC table! */
    for (i = 0; i < MAX_PARTNS_PDISK; i++)
	am_label->d_ptab[i].p_firstblk = am_label->d_ptab[i].p_numblks = 0;

    /* Fill the Amoeba label with partition table */
    for (pe = PCL_PCP(pclabel), n = 0, i = 0; i < PCL_NPART; pe++, i++)
    {
	if (pe->pp_sysind == PCP_AMOEBA)
	{
	    uint32 first, size;

	    memcpy((_VOIDSTAR) &first, (_VOIDSTAR) &pe->pp_first,
								sizeof (first));
	    memcpy((_VOIDSTAR) &size, (_VOIDSTAR) &pe->pp_size, sizeof (size));
	    am_label->d_ptab[n].p_firstblk = first + AMOEBA_BOOT_SIZE;
	    am_label->d_ptab[n].p_numblks = size - AMOEBA_BOOT_SIZE;
	    if (am_label->d_ptab[n].p_numblks < 0)
		am_label->d_ptab[n].p_numblks = 0;
	    n++;
	}
    }
    am_label->d_disktype[0] = '\0';
    am_label->d_magic = AM_DISK_MAGIC;
    (void) memset(am_label->d_filler, 0, FILLER_SIZE);
    am_label->d_chksum = 0;
    am_label->d_chksum = checksum((uint16 *) am_label);

    return 0;
}


static void
pr_dos_label(h_label)
    char *h_label;
{
    register struct pcpartition *pe;
    register int i, n;
    uint32 first, size;

    printf("%s\n%s\n",
"             ---first----   ----last----    -------sectors------",
"Part Active  Cyl Head Sec   Cyl Head Sec    Base    Last    Size");

    for (i = 0, pe = PCL_PCP(h_label), n = 0; i < PCL_NPART; i++, pe++) {
	if (pe->pp_sysind != PCP_AMOEBA) continue;

	memcpy((_VOIDSTAR) &first, (_VOIDSTAR) &pe->pp_first,
							    sizeof (first));
	memcpy((_VOIDSTAR) &size, (_VOIDSTAR) &pe->pp_size, sizeof (size));
	printf(" %c    %4s  %4d  %3d %3d  %4d  %3d %3d %7ld %7ld %7ld\n",
	    'a' + n++, pe->pp_bootind == PCP_ACTIVE ? "Yes" : "No ",
	    pe->pp_start_cyl + ((pe->pp_start_sec & 0xC0) << 2) +
		((pe->pp_start_head & 0xC0) << 4),
	    pe->pp_start_head & 0x3F, pe->pp_start_sec & 0x3F,
	    pe->pp_last_cyl + ((pe->pp_last_sec & 0xC0) << 2) +
		((pe->pp_last_head & 0xC0) << 4),
	    pe->pp_last_head & 0x3F, pe->pp_last_sec & 0x3F,
	    first, first + size, size);
    }
}


static int
dos_valid_label(h_label)
    char *h_label;
{
    return PCL_MAGIC(h_label);
}


/*ARGSUSED*/
static disk_addr
dos_partn_offset(amlabel)
    disklabel *amlabel;
{
    return 0;
}



host_dep	dos_host_dep = {
	dos_getlabel,
	dos_valid_label,
	dos_partn_offset,
	dos_labeldisk,
	pr_dos_label,
	LITTLE_E
};
