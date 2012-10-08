/*	@(#)sunos_dep.c	1.5	96/02/27 10:13:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * SunOS Disklabel Dependencies
 *
 *	This file contains the host operating system dependencies for
 *	dealing with SunOS disks.  Primarily they are concerned with the disk
 *	label of SUN OS.  They read/verify labels and copy the information
 *	about the disk into a struct known by Amoeba.
 *	At the end of the file is a struct containing the pointers to the
 *	routines defined.  This is the only external reference defined in
 *	this file.  All the routines are static.
 *
 * Author: Gregory J. Sharp, June 1992
 */

#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdio.h"
#include "string.h"
#include "module/disk.h"
#include "server/disk/conf.h"
#include "server/vdisk/disklabel.h"
#include "sunlabel.h"
#include "dsktab.h"
#include "host_dep.h"
#include "proto.h"

#define	MAX(a, b)	((a) > (b) ? (a) : (b))

extern char *		Prog;
extern int		Host_os;
extern dsktype		Dtypetab[];
extern int		Dtypetabsize;


/* Routines to convert SunOS labels to the local endian-ness */

/*ARGSUSED*/
static void
encode_sunlabel(s)
sun_label *	s;
{
    int	i;

    enc_s_be(&s->sun_apc);
    enc_s_be(&s->sun_gap1);
    enc_s_be(&s->sun_gap2);
    enc_s_be(&s->sun_intlv);
    enc_s_be(&s->sun_numcyl);
    enc_s_be(&s->sun_altcyl);
    enc_s_be(&s->sun_numhead);
    enc_s_be(&s->sun_numsect);
    enc_s_be(&s->sun_bhead);
    enc_s_be(&s->sun_ppart);
    enc_s_be(&s->sun_magic);
    enc_s_be(&s->sun_cksum);
    for (i = 0; i < SUN_MAX_PARTNS; i++)
    {
	enc_l_be(&s->sun_map[i].sun_cylno);
	enc_l_be(&s->sun_map[i].sun_nblk);
    }
}


/*ARGSUSED*/
static void
decode_sunlabel(s)
sun_label *	s;
{
    int	i;

    dec_s_be(&s->sun_apc);
    dec_s_be(&s->sun_gap1);
    dec_s_be(&s->sun_gap2);
    dec_s_be(&s->sun_intlv);
    dec_s_be(&s->sun_numcyl);
    dec_s_be(&s->sun_altcyl);
    dec_s_be(&s->sun_numhead);
    dec_s_be(&s->sun_numsect);
    dec_s_be(&s->sun_bhead);
    dec_s_be(&s->sun_ppart);
    dec_s_be(&s->sun_magic);
    dec_s_be(&s->sun_cksum);
    for (i = 0; i < SUN_MAX_PARTNS; i++)
    {
	dec_l_be(&s->sun_map[i].sun_cylno);
	dec_l_be(&s->sun_map[i].sun_nblk);
    }
}


/*
 * sun_valid_label
 *
 *	Routines to support host operating system dependencies which affect
 *	the disk driver.  Returns 1 for a valid label and 0 otherwise.
 *	Must decode to correct endian before inspecting the label.  If it is
 *	not a sun label put it back in original endian so that others can
 *	take a look at it.
 */

static int
sun_valid_label(label)
char *	label;
{
    sun_label *	slp;
    int		result;
    int16	check;

    slp = (sun_label *) label;
    check = checksum((uint16 *) slp);
    decode_sunlabel(slp);
    result = slp->sun_magic == SUN_MAGIC && check == 0;
    if (!result) /* Undo the decoding  if it wasn't a sun label */
	encode_sunlabel(slp);
    return result;
}


/*
 * copy_sun_amoeba
 *
 *	Copy data from a SunOS label to an amoeba label where the data
 *	in some way corresponds.
 */

static void
copy_sun_amoeba(slp, am_label)
sun_label *	slp;
disklabel *	am_label;
{
    int		i;

    am_label->d_geom.g_bpt = 0;		/* unknown */
    am_label->d_geom.g_bps = 0;		/* unknown */

    am_label->d_geom.g_numcyl = slp->sun_numcyl;
    am_label->d_geom.g_altcyl = slp->sun_altcyl;
    am_label->d_geom.g_numhead = slp->sun_numhead;
    am_label->d_geom.g_numsect = slp->sun_numsect;

/*
** when copying partition table we musn't copy more than there is or more
** than we can hold
*/
    i = MAX(SUN_MAX_PARTNS, MAX_PARTNS_PDISK);
    while (--i >= 0)
    {
	am_label->d_ptab[i].p_firstblk = slp->sun_map[i].sun_cylno *
					 slp->sun_numhead * slp->sun_numsect;
	am_label->d_ptab[i].p_numblks = slp->sun_map[i].sun_nblk;
    }
    am_label->d_magic = AM_DISK_MAGIC;
    (void) memset(am_label->d_filler, 0, FILLER_SIZE);
    am_label->d_chksum = 0;
    am_label->d_chksum = checksum((uint16 *) am_label);
}


/*
 * sun_getlabel
 *
 *	This routine reads a SunOS disk label and puts the partition and
 *	geometry info into an amoeba label.  Returns -1 if the read failed.
 *	Returns 1 if the label was invalid.  Returns 0 on success.
 */

static int
sun_getlabel(dkcap, label, am_label)
capability *	dkcap;		/* capability of disk to read */
char *		label;		/* optional return buffer for the label */
disklabel *	am_label;	/* amoeba label */
{
    char	h_label[D_PHYSBLKSZ];	/* buffer to read host OS label into */
    errstat	err;

    /* Read the sun OS label */
    err = disk_read(dkcap, D_PHYS_SHIFT, (disk_addr) 0, (disk_addr) 1, h_label);
    if (err != STD_OK)
    {
	fprintf(stderr, "%s: disk_read of block 0 failed (%s)\n",
							Prog, err_why(err));
	return -1;
    }

    /*
     * Sun_valid_label will decode from the right endian as a side-effect
     * if the label is valid.  If it isn't valid it remains in the original
     * endian.  That is why the memmove must be done after the sun_valid_label
     * call.
     */
    if (!sun_valid_label(h_label))
    {
	if (label != 0)
	    (void) memmove(label, h_label, D_PHYSBLKSZ);

	return 1;
    }

    if (label != 0)
	(void) memmove(label, h_label, D_PHYSBLKSZ);

    /* Fill in amoeba label */
    am_label->d_disktype[0] = '\0';	/* unknown */
    copy_sun_amoeba((sun_label *) h_label, am_label);
    return 0;
}


/*
 * sun_partn_offset
 *
 *	This is VERY IMPORTANT.  This tells the system how big the boot
 *	partition on the disk is.  Usually it is the first cylinder of the
 *	disk but some OS's use more!  The calling code assumes that the
 *	boot info starts at cylinder 0.  If this isn't the case then some
 *	work will need to be done in disklabel.c
 *	Returns:  number of SECTORS reserved for boot block!
 */

static disk_addr
sun_partn_offset(amlabel)
disklabel *	amlabel;
{
    return amlabel->d_geom.g_numhead * amlabel->d_geom.g_numsect;
}


/*
 * pr_sun_parttab
 *
 *      The the partition table of a SUNOS disk label
 */
 
static void
pr_sun_parttab(ptab)
sun_part_map *	ptab;	/* partition table to print */
{
    sun_part_map *	pp;
    int			i;

    printf("Partition  First Cylinder  Number of Sectors\n");
    for (pp = ptab, i = 0; i < SUN_MAX_PARTNS; pp++, i++)
    {
	printf("     %c      %8d       %8d\n",
				'a'+i, pp->sun_cylno, pp->sun_nblk);
    }
}


/*
 * enq_host_geometry
 *
 *	Asks the user to fill in the details of the disk goemetry.
 *	No effort is made to check that it is sensible data since for
 *	scsi disks it is irrelevant anyway.  If people get it wrong
 *	where it really matters their system won't work so they'll be
 *	back to do it properly.  It would be nice if we could pluck this
 *	data from a table for well known disks!
 *	Returns number of disk blocks available for use.
 *	NB the first cylinder is reserved for bootstrap so we don't count it.
 */

static disk_addr
enq_host_geometry(label, dkcap, type)
sun_label *	label;	/* the label to be filled in */
capability *	dkcap;	/* disk capability */
int		type;	/* the type of disk it is */
{
    errstat	err;
    disk_addr	numblks;
    long	chknblks;

    /* Initialise the goemetry. */
    if (type >= 0 && type < Dtypetabsize)
    {
	label->sun_numcyl = Dtypetab[type].geom.g_numcyl;
	label->sun_altcyl = Dtypetab[type].geom.g_altcyl;
	label->sun_numhead = Dtypetab[type].geom.g_numhead;
	label->sun_numsect = Dtypetab[type].geom.g_numsect;
    }
    /*
     * if the device has a command to get the capacity we call it else we
     * believe the info about cylinders/tracks/heads.
     */
    if ((err = disk_size(dkcap, D_PHYS_SHIFT, &numblks)) != STD_OK)
    {
	fprintf(stderr, "%s: cannot get disk size (%s)\n", Prog, err_why(err));
	numblks = -1;
    }

    /* ask user for disk geometry */
    printf("Please enter the disk geometry information\n");
    do
    {
	getfield16("Alternates per cyl.   : ", (int16 *) &label->sun_apc);
	getfield16("Bytes in gap1         : ", (int16 *) &label->sun_gap1);
	getfield16("Bytes in gap2         : ", (int16 *) &label->sun_gap2);
	getfield16("Interleave factor     : ", (int16 *) &label->sun_intlv);
	getfield16("# Cylinders (Used)    : ", (int16 *) &label->sun_numcyl);
	getfield16("# Alternate Cylinders : ", (int16 *) &label->sun_altcyl);
	getfield16("# Read/Write Heads    : ", (int16 *) &label->sun_numhead);
	getfield16("# Sectors per Track   : ", (int16 *) &label->sun_numsect);
	getfield16("Label location - head : ", (int16 *) &label->sun_bhead);
	getfield16("Label location - part : ", (int16 *) &label->sun_ppart);

	/* Now check that the capacity claimed by the disk is not less than
	 * what the user claims in the geometry
	 */
	chknblks = label->sun_numcyl * label->sun_numsect * label->sun_numhead;
	if (numblks != -1 && chknblks > numblks)
	{
	    printf("WARNING:  Total blocks according to geometry (%d) is\n",
								    chknblks);
	    printf("more than total number of blocks available on disk (%d)\n",
								    numblks);
	}
    } while ((numblks != -1 && chknblks > numblks) ||
			!confirm("Is all the above information correct"));
    /* The value returned is in fact 1 cylinder less than available since
     * we have reserved the first cylinder for boot information
     */
    if (numblks == -1)
	return chknblks - label->sun_numsect * label->sun_numhead;
    else
	return numblks - label->sun_numsect * label->sun_numhead;
}


/*
 * partn_inconsistent
 *
 *	Checks to see if the given partition table is consistent.
 *	Overlaps are allowed but partition must fit on the disk.
 *	Partitions whose firstblk field are negative are ignored.
 *	Returns 1 if inconsistency is found and 0 otherwise.
 */

static int
partn_inconsistent(pp, numblks, scale)
sun_part_map *	pp;		/* new entry to be checked */
disk_addr	numblks;	/* total # blocks on disk */
long		scale;
{
    disk_addr	firstblk;

    /* Ignore null partition */
    if (pp->sun_nblk != 0)
    {

	/* Make sure partition fits on the disk */
	if ((firstblk = pp->sun_cylno * scale) > numblks)
	{
	    printf("First block of partition is not on the disk\n");
	    return 1;
	}
	else
	    if (firstblk + pp->sun_nblk > numblks)
	    {
		printf("Partition has too many sectors to fit on the disk.\n");
		return 1;
	    }
    }
    return 0;
}


/*
 * enq_host_partition_table
 *
 *	Reads the description of a partition.
 *	Checks that the partitions actually fit on the disk, etc.
 */

static void
enq_host_partition_table(ptab, numblks, scale, type)
sun_part_map *	ptab;		/* pointer to start of partition table */
disk_addr	numblks;	/* total # usable blocks on disk */
long		scale;		/* converstion from cylno to block number */
int		type;		/* the type of disk */
{
    int			i;		/* loop counter */
    sun_part_map *	pp;		/* pointer to current table entry */

    /* Initialise the partition table.  Defaults some day? */
    if (type >= 0 && type < Dtypetabsize)
	for (pp = ptab, i = 0; i < SUN_MAX_PARTNS; pp++, i++)
	{
	    pp->sun_cylno = -1;
	    pp->sun_nblk = -1;
	}

    /* Now ask the user where to put them */
    do
    {
	printf("Please enter the partition table information\n");
	for (pp = ptab, i = 0; i < SUN_MAX_PARTNS; pp++, i++)
	    do
	    {
		printf("\nPartition  %c: ", 'a'+i);
		getfield32("First Cylinder    : ", &pp->sun_cylno);
		getfield32("              Number of Sectors : ", &pp->sun_nblk);
	    } while (partn_inconsistent(pp, numblks, scale));

	/* Now print a summary of partition table */
	printf("Partition Table Summary\n");
	printf("Number of Sectors on Disk = %ld\n", numblks);
	pr_sun_parttab(ptab);
    } while (!confirm("Is the partition table ok"));
}


/*
 * sun_labeldisk
 *
 *	Puts a SUNOS sticky label on the specified disk.  If the label already
 *	has a label and the user wants to modify it rather than make a new
 *	label then modify is set to -1.  Otherwise modify is 0.
 */

static void
sun_labeldisk(dp, type)
dsktab *	dp;		/* pointer to disk to be labelled */
int		type;		/* -1 if current label is to be modified */
{
    /* Label of disk to be labelled. It is global so that it is zeroed */
    static sun_label	Label;
    disk_addr	total_blocks;	/* # blocks available for use */
    sun_label	slabel;

    if (type == -1)
	(void) memmove((char *) &Label, dp->plabel, sizeof (sun_label));

    /* Get details to write into disk label */
    total_blocks = enq_host_geometry(&Label, &dp->diskcap, type);
    printf("\nThere are %ld sectors of %d bytes available (%d cylinders).\n",
				total_blocks, D_PHYSBLKSZ, Label.sun_numcyl-1);
    enq_host_partition_table(Label.sun_map, total_blocks,
			(long) (Label.sun_numhead * Label.sun_numsect), type);

    /* Install magic number and checksum */
    Label.sun_magic = SUN_MAGIC;
    (void) memset(Label.sun_pad, 0, SUN_FILLER_SIZE);
    Label.sun_cksum = 0;
    Label.sun_cksum = checksum((uint16 *) &Label);

    /* Write out the label to disk */
    (void) memmove(dp->plabel, (char *) &Label, sizeof (sun_label));
    copy_sun_amoeba(&Label, &dp->amlabel);
    if (confirm("Last chance.  Do you really want to label this disk"))
    {
	(void) memcpy(&slabel, dp->plabel, sizeof (sun_label));
	encode_sunlabel(&slabel);
	if (writeblock(&dp->diskcap, dp->name, (disk_addr) 0,
						(char *) &slabel) != STD_OK)
	{
	    printf("Disk label has not been written.\n");
	}
	else
	{
	    dp->labelled = Host_os;
	    printf("Disk label has been written.\n");
	}
    }
    else
	printf("Label was not written.\n");
}


/*
 * pr_host_label
 *
 *	Prints a SUNOS label
 */

static void
sun_print_label(buf)
char *	buf;
{
    sun_label *	lp;

    lp = (sun_label *) buf;
    printf("      SUNOS Disk Label\n");
    printf("Alternates per cyl  : %d\n", lp->sun_apc);
    printf("Bytes in Gap1       : %d\n", lp->sun_gap1);
    printf("Bytes in Gap2       : %d\n", lp->sun_gap2);
    printf("Interleave Factor   : %d\n", lp->sun_intlv);
    printf("Number of Cylinders : %d\n", lp->sun_numcyl);
    printf("Alternate Cylinders : %d\n", lp->sun_altcyl);
    printf("Number of Heads     : %d\n", lp->sun_numhead);
    printf("Sectors per track   : %d\n", lp->sun_numsect);
    printf("Label head          : %d\n", lp->sun_bhead);
    printf("Physical Partition  : %d\n", lp->sun_ppart);
    printf("Partition Table\n");
    pr_sun_parttab(lp->sun_map);
}


host_dep	sunos_host_dep = {
	sun_getlabel,
	sun_valid_label,
	sun_partn_offset,
	sun_labeldisk,
	sun_print_label,
	BIG_E
};
