/*	@(#)merg_vdsks.c	1.4	96/02/27 10:12:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * merg_disks
 *
 *	This file contains the code for merging virtual disks.
 *
 * Author: Greg Sharp, June 1992
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdio.h"
#include "stdlib.h"
#include "module/disk.h"
#include "disk/conf.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "vdisk/vdisk.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "proto.h"

extern vdisktab *	Vdtab;
extern char *		Prog;
extern char *		Hostname;

/*
 * repeated
 *
 *	Goes through the array of vdisktab pointers looking to see if the
 *	last pointer in the array appears anywhere else in the array.
 *	Returns 1 if it does and 0 otherwise.
 */

static int
repeated(vtab, many)
vdisktab *	vtab[];	/* array of pointers to vdisktab entries */
int		many;	/* number of items in the array - 1 */
{
    int		i;

    for (i = 0; i < many; i++)
	if (vtab[i] == vtab[many])
	{
	    printf("You've already included that disk\n");
	    return 1;
	}
    return 0;
}


/*
 * rmlist
 *
 *	Delete entry 'p' from the linked list 'list'.
 */

static vdisktab *
rmlist(list, p)
vdisktab *	list;
vdisktab *	p;
{
    vdisktab *	q;

    if (p == 0 || list == 0)
	return list;
    if (p == list)
    {
	list = list->v_next;
	free(p);
	return list;
    }
    q = list;
    while (q != 0 && q->v_next != p)
	q = q->v_next;
    if (q == 0)
	return list;		/* p not in list */
    q->v_next = q->v_next->v_next;
    free(p);
    return list;
}


/*
 * merge_vdisks
 *
 *	Generates a list of known virtual disks (usually one per physical
 *	disk partition) and then asks which virtual disks should be merged
 *	with which other virtual disks.
 *	At the end the physical disk labels are written to disk reflecting
 *	the newly merged virtual disks.
 */

void
merge_vdisks _ARGS(( void ))
{
    vdisktab *	vp[MAX_PARTNS_VDISK];	/* pointers to related table entry */
    objnum	vdisknum;	/* virtual disk number */
    int		many;		/* number of disks to be merged */
    vdisktab	newvdisk;	/* buffer for new merged virtual disk */
    part_tab *	pp;		/* used to point to phys. partition entry */
    part_list *	p;		/* pointer to physical disk table entry */
    int		part;		/* partition number in physical label */
    int		i;		/* loop counter */
    vdisktab *	vtab;		/* start of linked list of virtual disks */

    /* Ask user for the disks to be merged */
    display_vdisks();
    vtab = Vdtab;
    if (vtab == 0)
    {
	/* error message will be printed by display_vdisks() */
	return;
    }

    do
    {
	/*
	 * Get virtual disks in the set - these all get same cap as the first
	 * disk given
	 */
	printf("\nPlease enter the numbers of the virtual disks to be merged");
	printf("\nin the desired order of merging (one number per line).\n");
	printf("Enter 0 or press return to terminate list\n");
	newvdisk.v_many = 0;
	newvdisk.v_numblks = 0;
	newvdisk.v_next = 0;
	for (many = 0; many < MAX_PARTNS_VDISK;)
	{
	    do /* until vdisknum == 0 or a valid, non repeated vdisk number */
	    {
		printf("Virtual disk for piece %d: ", many);
	    } while ((vdisknum = getnumber("")) != 0 &&
			(((vp[many] = find_entry(vtab, vdisknum)) == 0) ||
			 repeated(vp, many)));

	    if (vdisknum == 0)
		break;		/* no more virtual disks to add */

	    /*
	     * Add stuff from vp to newvdisk; 0 is special case - must copy cap
	     */
	    if (many == 0)
		newvdisk.v_cap = vp[many]->v_cap;
	    for (i = 0; i < vp[many]->v_many; i++)
	    {
		newvdisk.v_many++;
		newvdisk.v_parts[many+i] = vp[many]->v_parts[i];
		newvdisk.v_numblks += vp[many]->v_parts[i].i_numblks;
	    }
	    many += vp[many]->v_many;
	}

	/*
	 * If the virtual disk is made of more than one partition write labels
	 */
	if (many > 1 && confirm("Are you sure you want merge all these"))
	{
	    /*
	     * Copy newvdisk to 0th disk and get rid of the other virtual disks
	     */
	    newvdisk.v_next = vp[0]->v_next;
	    *(vp[0]) = newvdisk;
	    for (i = 1; i < many; i++)
		vtab = rmlist(vtab, vp[i]);
	    Vdtab = vtab;

	    /* Go through and adjust the physical disk labels */
	    for (i = 0; i < newvdisk.v_many; i++)
	    {
		p = newvdisk.v_parts[i].i_physpart;
		part = newvdisk.v_parts[i].i_part;
		pp = &p->label.d_ptab[part];
		pp->p_piece = i;
		pp->p_many = newvdisk.v_many;
		pp->p_cap = newvdisk.v_cap;
		(void) memset(p->label.d_filler, 0, FILLER_SIZE);
		/* Calculate checksum! */
		p->label.d_chksum = 0;
		p->label.d_chksum = checksum((uint16 *) &p->label);
		/* Write out the label */
		if (write_amlabel(&p->diskcap, p->name, p->labelblock,
							&p->label) != STD_OK)
		{
		    return;
		}
	    }
	    /*
	     * At this point we should renumber the vdisks in Vdtab.  They
	     * are numbered starting at 1.
	     */
	}
	display_vdisks();
    } while (confirm("Do you want to make another multi-volume disk"));
}


/*
 * do_unmerge
 *
 *	Unmerge the specified vdisk - replace it with several vdisks, one
 *	for each original component.
 */

do_unmerge(vp, nextnum)
vdisktab *	vp;		/* pointer to entry to be unmerged */
objnum	*	nextnum;	/* next free vdisk number */
{
    int		i;
    disklabel *	dp;
    part_list *	plp;
    part_tab *	ptp;
    vpart *	vptr;
    errstat	err;

    printf("UNMERGE\n");
    for (vptr = vp->v_parts, i = 0; i < vp->v_many; vptr++, i++)
    {

	/* Modify the corresponding Partition to be a single vdisk */
	plp = vptr->i_physpart;
	dp = &plp->label;
	ptp = &dp->d_ptab[vptr->i_part];
	ptp->p_piece = 0;
	ptp->p_many = 1;
	uniqport(&ptp->p_cap.cap_priv.prv_random);
	(void) prv_encode(&ptp->p_cap.cap_priv, *nextnum, PRV_ALL_RIGHTS,
			&ptp->p_cap.cap_priv.prv_random);
	dp->d_chksum = 0;
	dp->d_chksum = checksum((uint16 *) dp);

	/* Rewrite Partition label */
	err = write_amlabel(&plp->diskcap, plp->name, plp->labelblock, dp);
	if (err != STD_OK)
	{
	    fprintf(stderr, "%s: unmerge failed (%s)\n", Prog, err_why(err));
	    exit(-10);
	}

	/* Add new vdisk */
	Vdtab = new_entry(Vdtab, *nextnum, vptr->i_physpart, vptr->i_part);
	(*nextnum)++;
    }

    /* Remove old entry from list */
    rmlist(Vdtab, vp);
}


void
unmerge_vdisks _ARGS(( void ))
{
    vdisktab *	vtab;
    int		num_merged;	/* Number of merged vdisks in the system */
    objnum	dknum;
    objnum	lastnum;	/* last allocated vdisk number */

    vtab = Vdtab;
    if (vtab == 0)
    {
	printf("\nNo virtual disks are defined on host `%s'!\n", Hostname);
	return;
    }

    /* Find if any of the disks are merged */
    for (lastnum = 0, num_merged = 0; vtab != 0; vtab = vtab->v_next)
    {
	objnum x;

	if (vtab->v_many > 1)
	    num_merged++;
	x = prv_number(&vtab->v_cap.cap_priv);
	if (x > lastnum)
	    lastnum = x;
    }
    if (num_merged == 0)
    {
	printf("There are no merged virtual disks to unmerge\n");
	return;
    }

    lastnum++;
    /* Let them unmerge to their heart's content */
    do
    {
	display_vdisks();
	dknum = getnumber("\nWhich vdisk do you wish to unmerge [0 to exit]? ");
	if (dknum == 0)
	    return;
	vtab = find_entry(Vdtab, dknum);
	if (vtab == 0)
	{
	    printf("There is no vdisk %d\n", dknum);
	}
	else
	{
	    if (vtab->v_many <= 1)
	    {
		printf("Vdisk %d has only 1 part - it can't be unmerged\n",
									dknum);
	    }
	    else
	    {
		do_unmerge(vtab, &lastnum);
		num_merged--;
	    }
	}
    } while (num_merged > 0);

    printf("Renumbering vdisks!\n");
    renumber_vdisks();

    display_vdisks();
    printf("\nThere are no more concatenated disks!\n");
    pause();
}
