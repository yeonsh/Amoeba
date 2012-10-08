/*	@(#)partnlabel.c	1.8	96/02/27 10:12:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * label_partns
 *
 *	Code to label physical disk partitions with an Amoeba label.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"
#include "disk/conf.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "vdisk/vdisk.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "host_dep.h"
#include "proto.h"

extern int	   Dtypetabsize;
extern char *	   Prog;	/* Name of this program as per argv[0] */
extern int	   Host_os;	/* Selector for the Host base for the disk */
extern host_dep *  Host_p[];	/* Table of routines for host os */
extern char *	   Hostname;	/* Name of host being labelled */
extern dsktab *	   Pdtab;	/* Physical disk table */
extern part_list * Partitions;	/* List of physical partitions available */


/*
 * warn_overlap
 *
 *	Lets the user know if the current partition overlaps other partitions.
 */

static int
warn_overlap(ptab, pp)
part_tab *	ptab;	/* pointer to start of partition table */
part_tab *	pp;	/* pointer to entry to be checked against others */
{
    int		i;
    int		error;

    error = 0;
    for (i = 0; i < MAX_PARTNS_PDISK; ptab++, i++)
	if (ptab != pp && ptab->p_firstblk >= 0 && ptab->p_numblks > 0 &&
		((pp->p_firstblk < ptab->p_firstblk &&
		  pp->p_firstblk + pp->p_numblks > ptab->p_firstblk) ||
		(ptab->p_firstblk <= pp->p_firstblk &&
		 ptab->p_firstblk + ptab->p_numblks > pp->p_firstblk)))
	{
	    printf("This partition overlaps partition %c\n", 'a'+i);
	    error = 1;
	}
    return error;
}


/*
 * new_phys_partn
 *
 *	Adds a physical partition to the linked list of partitions 'plist'.
 *	Returns a pointer to the new list.
 */

static part_list *
new_phys_partn(plist, lb, dp, labelblk, firstblk, size, partn_num)
part_list *	plist;		/* list of physical partitions */
disklabel *	lb;		/* amoeba label for the partition */
dsktab *	dp;		/* pointer to physical disk data */
disk_addr	labelblk;	/* pointer to partition label */
disk_addr	firstblk;	/* pointer to start of partition */
disk_addr	size;		/* size of partition in 0.5K blocks */
int		partn_num;	/* partition number in dp */
{
    part_list *	newpl;

    if ((newpl = (part_list *) malloc(sizeof (part_list))) == 0)
	fprintf(stderr, "%s: new_phys_partn: malloc failed\n", Prog);
    else
    {
	newpl->diskcap = dp->diskcap;
	(void) strcpy(newpl->name, dp->name);
	newpl->label = *lb;
	newpl->labelled = am_partn_label(lb);
	newpl->labelblock = labelblk;
	newpl->firstblock = firstblk;
	newpl->size = size;
	newpl->partnum = partn_num;
	newpl->next = plist;
	plist = newpl;
    }
    return plist;
}


/*
 * free_partn_list
 *
 *	Free any malloced data structures used by a previous incarnation of
 *	the list of physical partitions
 */

static void
free_partn_list(plist)
part_list *	plist;
{
    part_list *	pnext;

    while (plist != 0)
    {
	pnext = plist->next;
	free(plist);
	plist = pnext;
    }
}


/*
 * make_partn_list
 *
 *	Make a linked list of all physical partitions present on the system.
 *	A partition is not added if its first block cannot be read.
 *	Any existing list of physical partitions is thrown away after the
 *	new list is constructed.
 */

void
make_partn_list _ARGS(( void ))
{
    disklabel	vlabel;	/* vdisk label on partition */
    disk_addr	lblk;	/* label block of disk partition */
    dsktab *	dp;	/* pointer to current entry in disk list */
    part_tab *	pp;	/* pointer to partition table entry */
    part_list *	pltab;	/* start of partition list */
    int		i;
    errstat	err;

    /*
     * For each disk, if it is labelled with the Host_os label, put its
     * partitions on the partition list if the partition's first block
     * can be read.
     */
    pltab = 0;
    for (dp = Pdtab; dp != 0; dp = dp->next)
    {
	if (dp->labelled == Host_os)
	{
	    pp = dp->amlabel.d_ptab; /* start of physical partition table */
	    for (i = 0; i < MAX_PARTNS_PDISK; pp++, i++)
	    {
		if (pp->p_numblks != 0)
		{
		    /* The partition is real - ie. has some blocks in it */
		    lblk = pp->p_firstblk +
			      (*Host_p[Host_os]->f_partn_offset)(&dp->amlabel);
		    err = read_amlabel(&dp->diskcap, dp->name, lblk, &vlabel);
		    if (err == STD_OK)
		    {
			/* A hack - we force the geometry info in our copy of
			 * the label to be correct.  This saves as some work
			 * later on if we need to replace the label.
			 */
			vlabel.d_geom = dp->amlabel.d_geom;
			(void) strcpy(vlabel.d_disktype, dp->amlabel.d_disktype);
			pltab = new_phys_partn(pltab, &vlabel, dp, lblk,
					    pp->p_firstblk, pp->p_numblks, i);
		    }
		    else
		    {
			fprintf(stderr,
		"%s: make_partn_list disk_read of %s block 0x%x failed (%s)\n",
				Prog, dp->name, lblk, err_why(err));
		    }
		}
	    }
	}
    }

    /*
     * If there is already a partition list throw it away before making
     * a new one.
     */
    if (Partitions != 0)
	free_partn_list(Partitions);
    Partitions = pltab;
}


void
display_partn_list _ARGS(( void ))
{
    part_list *	p;

    if (Partitions == 0)
    {
	printf("There are no known physical partitions on host %s\n", Hostname);
	return;
    }

    printf("Currently known partitions:\n");
    printf("  Disk     Partn  Begin Sector  End Sector   Size (sectors)\n");

    for (p = Partitions; p != 0; p = p->next)
    {
	printf("%s:    %c    %8ld      %8ld      (%8ld)\n", p->name,
	    'a' + p->partnum, p->labelblock, p->labelblock + p->size, p->size);
    }
    pause();
}


/*
 * pr_am_parttab
 *
 *	The the partition table of an Amoeba disk label
 */

static void
pr_am_parttab(ptab)
part_tab *	ptab;
{
    part_tab *	pp;
    int		i;

    printf("Partition  First Sector  Number of Sectors\n");
    for (pp = ptab, i = 0; i < MAX_PARTNS_PDISK; pp++, i++)
    {
	printf("    %c      %8d      %8d\n",
					'a'+i, pp->p_firstblk, pp->p_numblks);
    }
}


/*
 * inconsistent
 *
 *	Checks to see if the given partition table is consistent.
 *	Overlaps are not allowed and partition must fit on the disk.
 *	Partitions whose firstblk field are negative are ignored.
 *	Returns 1 if inconsistency is found and 0 otherwise.
 */

static int
inconsistent(pp, ptab, numblks)
part_tab *	pp;		/* new entry to be checked */
part_tab *	ptab;		/* start of partition table */
disk_addr	numblks;	/* total # blocks on disk */
{
    /* Ignore null partition */
    if (pp->p_numblks == 0)
	return 0;

    /* Make sure partition fits on the disk */
    if (pp->p_firstblk > numblks)
    {
	printf("First block of partition is not on the disk\n");
	return 1;
    }
    else
	if (pp->p_firstblk + pp->p_numblks > numblks)
	{
	    printf("Partition has too many sectors to fit on the disk.\n");
	    return 1;
	}

    /*
     * Go through and see if it overlaps other partitions
     * but don't compare pp with a) itself, b) uninitialised partitions or
     * c) null partitions.
     */
    if (warn_overlap(ptab, pp) && !confirm("Go on to next partition"))
	return 1;
    return 0;
}


/*
 * enquire_partition_table
 *
 *	Gets the description of a partition table for an Amoeba partition.
 *	Checks that the partitions actually fit on the disk, etc.
 * NB:  due to hysterical raisins it is necessary to enforce that all blocks
 *      of a subpartition are used.  A little draconian, but so be it.
 */

static void
enquire_partition_table(ptab, numblks, type)
part_tab *	ptab;		/* pointer to start of partition table */
disk_addr	numblks;	/* total # usable blocks on disk */
int		type;		/* the type of disk it is */
{
    static capability	Nullcap;

    int		i;		/* loop counter */
    part_tab *	pp;		/* pointer to current table entry */
    disk_addr	prevfblk;	/* previous first block value */
    disk_addr	prevnblk;	/* previous number of blocks value */
    disk_addr	total_used;	/* number of disk blocks allocated so far */

    /* Initialise the partition table */
    if (type >= 0 && type < Dtypetabsize)
    {
	for (pp = ptab, i = 0; i < MAX_PARTNS_PDISK; pp++, i++)
	{
	    pp->p_firstblk = -1;
	    pp->p_numblks = -1;
	    pp->p_piece = 0;
	    pp->p_many = 1;
	    pp->p_cap = Nullcap;	/* can't sensibly generate a cap yet */
	}
    }

    /* Now ask the user where to put them */
    do
    {
	printf("Please enter the partition table information\n");
	
	for (;;)
	{
	    for (total_used = 0, pp = ptab, i = 0;
		    i < MAX_PARTNS_PDISK && total_used < numblks; pp++, i++)
	    {
		do
		{
		    prevfblk = pp->p_firstblk;
		    prevnblk = pp->p_numblks;
		    printf("\nPartition %c: ", 'a'+i);
		    getfield32("First Sector      : ", &pp->p_firstblk);
		    getfield32("             Number of Sectors : ",
								&pp->p_numblks);
		    /* If we change an extant label - delete it from vdisk! */
		    if (type < 0 &&
			(prevfblk != pp->p_firstblk ||
			 prevnblk != pp->p_numblks))
		    {
			pp->p_piece = 0;
			pp->p_many = 1;
			pp->p_cap = Nullcap;
		    }
		} while (inconsistent(pp, ptab, numblks));
		total_used += pp->p_numblks;
	    }

	    /* If we haven't used all the blocks then enforce it */
	    if (total_used == numblks)
		break;
	    printf("You used %d of the %d available blocks.\n",
							total_used, numblks);
	    printf("Please use up the remaining %d blocks.\n",
							numblks - total_used);
	}

	/* Fill in any empty fields if whole disk was used up */
	for (; i < MAX_PARTNS_PDISK; pp++, i++)
	{
	    pp->p_firstblk = 0;
	    pp->p_numblks = 0;
	}

	/* Now print a summary of partition table */
	printf("Partition Table Summary\n");
	printf("Number of Sectors on Disk = %ld\n", numblks);
	pr_am_parttab(ptab);
    } while (!confirm("Is the partition table ok"));
}


/*
 * make_am_partn
 *
 *	Enquires for the sub-partitioning information for the physical
 *	partition and then writes an Amoeba label to block 0 of the
 *	physical partition.  If the partition already has a label and is
 *	being relabelled then "labelled" is set.
 */

static void
make_am_partn(pp, labelled)
part_list *	pp;		/* current disk partition */
int		labelled;	/* the partition aleady has an vdisk label */
{
    int		j;

    /*
     * At this point the label present should be a copy of the disk
     * block found at the start of the partition but with the geometry
     * already correct due to a hack in make_partn_list.  We need to fill
     * it in with (the rest of) an Amoeba label.  (Not a vdisk label!)
     *
     * Ask user for sub-partitioning
     */
    printf("You need to give the sub-partitioning of this partition.\n");
    if (labelled)
    {
	/*
	 * All extant sub-partitions must be made relative to virtual start of
	 * partition before enquiring for partition offsets
	 */
	for (j = 0; j < MAX_PARTNS_PDISK; j++)
	    pp->label.d_ptab[j].p_firstblk -= pp->firstblock;
    }

    enquire_partition_table(pp->label.d_ptab, pp->size, labelled ? -1 : 0);

    /*
     * All sub-partitions must be relative to start of physical partition
     */
    for (j = 0; j < MAX_PARTNS_PDISK; j++)
	pp->label.d_ptab[j].p_firstblk += pp->firstblock;

    pp->label.d_magic = AM_DISK_MAGIC;
    /* We set the filler to 0 to ensure that we don't get problems with
     * the check sum if we try to decode on different endian machine
     */
    (void) memset(pp->label.d_filler, 0, FILLER_SIZE);
    /*
     * We have to set the check sum in the internal copy so that the
     * illusion of a correct Amoeba label is maintained later.  The
     * checksum for the version written to disk is calculated by write_amlabel
     */
    pp->label.d_chksum = 0;
    pp->label.d_chksum = checksum((uint16 *) &pp->label);

    if (write_amlabel(&pp->diskcap, pp->name, pp->labelblock, &pp->label) != STD_OK)
    {
	fprintf(stderr, "%s: make_am_partn failed\n", Prog);
    }
    pp->labelled = 1;
}


/*
 * label_partns
 *
 *	Goes through the list of physical partitions and for each checks if
 *	it begins with an amoeba label or not, then enquires if the partition
 *	should be set up for use by amoeba or no longer be used by amoeba
 */

void
label_partns _ARGS(( void ))
{
    static char	Null_label[D_PHYSBLKSZ];   /* zeroed block = empty label */
    part_list *	plist;

    if (Partitions == 0)
    {
	printf("There are no physical partitions to make vdisks from.\n");
	return;
    }
    printf("\nYou must now decide which partitions to allocate to Amoeba.\n");
    /* For each physical disk partition on plist */
    for (plist = Partitions; plist != 0; plist = plist->next)
    {
	printf("\nDisk `%s' ", plist->name);
	printf("Partition %c", 'a' + plist->partnum); 
	printf(" (%ld blocks):\n", plist->size);
	if (am_partn_label(&plist->label) &&
			    plist->size == total_blocks(plist->label.d_ptab))
	{
	    printf("This partition is being used for Amoeba.\n");
	    if (confirm("Should it stop being an Amoeba partition"))
	    {
		/* Write a zero block! */
		(void) memcpy(&plist->label, Null_label, D_PHYSBLKSZ);
		(void) writeblock(&plist->diskcap, plist->name,
						plist->labelblock, Null_label);
		plist->labelled = 0;
	    }
	    else
	    {
		if (confirm("Do you want to resubpartition this partition"))
		    make_am_partn(plist, 1);
	    }
	}
	else
	{
	    if (confirm("Can I use this partition for Amoeba"))
		make_am_partn(plist, 0);
	}
    }
    /* Rebuild the vdisk list */
    make_vdisk_list();
}
