/*	@(#)virlabel.c	1.5	96/02/27 10:13:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * virlabel
 *
 *	Code to label virtual disks.
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

extern dsktype		Dtypetab[];
extern int		Dtypetabsize;
extern host_dep *	Host_p[];
extern int		Host_os;
extern char *		Prog;
extern part_list *	Partitions;
extern vdisktab *	Vdtab;



/*
 * new_entry
 *
 *	Add new virtual disk to linked list.  List is sorted on increasing
 *	order of virtual disk number, with new entries with the same number
 *	being added at the end of the group (which should only happen for 0),
 *	so that the oldest is at the front of the list.
 */

vdisktab *
new_entry(vdtab, vdnum, plp, part)
vdisktab *	vdtab;		/* start of virtual disk list */
objnum		vdnum;		/* number of new virtual disk */
part_list *	plp;		/* pointer to physical disk entry */
int		part;		/* partition number */
{
    vdisktab *	vp;		/* pointer to new entry */
    vdisktab *	vtbp;		/* pointer to current position in table */
    part_tab *	pp;		/* pointer to physical partition */


    if ((vp = (vdisktab *) malloc(sizeof (vdisktab))) == 0)
	fprintf(stderr, " %s: new_entry: malloc failed\n", Prog);
    else  /* fill in vp and insert in the correct place in the list */
    {
	pp = &plp->label.d_ptab[part];
	vp->v_cap = pp->p_cap;
	vp->v_many = pp->p_many;
	vp->v_numblks = pp->p_numblks;
	vp->v_parts[pp->p_piece].i_physpart = plp;
	vp->v_parts[pp->p_piece].i_part = part;
	vp->v_parts[pp->p_piece].i_firstblk = pp->p_firstblk;
	vp->v_parts[pp->p_piece].i_numblks = pp->p_numblks;
	/* Should new entry go on front of list? */
	if (vdtab == 0 || vdnum < prv_number(&vdtab->v_cap.cap_priv))
	{
	    vp->v_next = vdtab;
	    vdtab = vp;
	}
	else
	{
	    for (vtbp = vdtab; ; vtbp = vtbp->v_next)
	    {
		/*
		 * If at end of list or it belongs immediately after vtbp
		 * then insert it in the list.
		 */
		if (vtbp->v_next == 0 ||
			vdnum < prv_number(&vtbp->v_next->v_cap.cap_priv))
		{
		    vp->v_next = vtbp->v_next;
		    vtbp->v_next = vp;
		    break;
		}
	    }
	}
    }
    return vdtab;
}


/*
 * find_entry
 *
 *	Go through the virtual disk table and find the entry corresponding
 *	to the virtual disk number given.
 */

vdisktab *
find_entry(vtab, num)
vdisktab *	vtab;	/* start of virtual disk table */
objnum		num;
{
    while (vtab && num != prv_number(&vtab->v_cap.cap_priv))
	vtab = vtab->v_next;
    return vtab;
}


/*
 * free_vdisk_list
 *
 *	Free any malloced data structures used by a previous incarnation of
 *	the list of physical partitions
 */

static void
free_vdisk_list(vlist)
vdisktab *	vlist;
{
    vdisktab *	vnext;

    while (vlist != 0)
    {
	vnext = vlist->v_next;
	free(vlist);
	vlist = vnext;
    }
}


/*
 * make_vdisk_list
 *
 *	Goes through the known physical partitions and reads the amoeba
 *	partition label for each partition, picks out the virtual disks
 *	that have been defined and gives them all a number if they haven't
 *	got one already.  It also invents a port for the virtual disks if
 *	none has yet been defined.
 */

void
make_vdisk_list _ARGS(( void ))
{
    int		i;		/* sub-partition loop counter */
    int		isvdisk;	/* if there is a defined vdisk 1 else 0 */
    objnum	nextnum;	/* next virtual disk number to allocate */
    part_list *	plp;		/* physical disk table pointer */
    part_tab *	spp;		/* pointer to partition table of sub-partn */
    objnum      vdisknum;       /* virtual disk number */
    port	vdport;		/* port used by all virtual disks */
    vdisktab *	vdtab;		/* head of linked list of virtual disks */
    vdisktab *	vp;		/* pointer to virtual disk entry */
    vpart *	vpp;		/* virtual disk partition table pointer */
    errstat	err;

    /*
     * Pass 1: Find all vdisks, put into list sorted by virtual disk number.
     *	   Also find the port if there is one.  This will be on the first
     *	   disk in the list with a non-zero virtual disk number.
     *	   Virtual disk number 0 means that it is a new partition not
     *	   previously allocated to a virtual disk.
     */
    /* Initially there are no virtual disks in the list */
    vdtab = 0;
    isvdisk = 0;
    nextnum = 1;

    /*
     * For each physical partition, look at each sub-partition to see if it
     * belongs to a virtual disk.
    */
    for (plp = Partitions; plp != 0; plp = plp->next)
    {
	if (!plp->labelled)
	    continue;
	spp = plp->label.d_ptab;	/* start of sub-partition table */
	/* For each sub-partition */
	for (i = 0; i < MAX_PARTNS_PDISK; spp++, i++)
	    if (spp->p_numblks > 1)
	    {
		/* Initially capabilities are null! */
		if ((vdisknum = prv_number(&spp->p_cap.cap_priv)) == 0)
		    vdtab = new_entry(vdtab, vdisknum, plp, i);
		else
		{
		    /* Get port from first extant vdisk we find */
		    if (!isvdisk)
		    {
			isvdisk = 1;
			vdport = spp->p_cap.cap_port;
		    }
		    /*
		     * nextnum starts after the highest vdisknum on our current
		     * system.  It is used in pass 2 to number new vdisks.
		     */
		    if (vdisknum >= nextnum)
			nextnum = vdisknum + 1;
		    /*
		     * See if this partition is just part of a vdisk we have
		     * already found.
		     */
		    if ((vp = find_entry(vdtab, vdisknum)) == 0)
			vdtab = new_entry(vdtab, vdisknum, plp, i);
		    else
		    {
			vp->v_numblks += spp->p_numblks;
			vp->v_parts[spp->p_piece].i_physpart = plp;
			vp->v_parts[spp->p_piece].i_part = i;
			vp->v_parts[spp->p_piece].i_firstblk = spp->p_firstblk;
			vp->v_parts[spp->p_piece].i_numblks = spp->p_numblks;
			/* Force consistency */
			spp->p_cap = vp->v_cap;
			if (spp->p_many != vp->v_many)
			    printf("Inconsistency in many value!\n");
		    }
			    
		}
	    }
	
	/* If we couldn't find an existing vdisk then */
	if (!isvdisk)	/* we must invent a port for the virtual disks */
	    uniqport(&vdport);
    }

    /*
     * Pass 2: Put vdport into all vdisks.
     *	   Invent vdisk number, random part plus give all rights for the
     *	   uninitialised virtual disks.  Also adjust physical labels.
     */
    for (vp = vdtab; vp != 0; vp = vp->v_next)
    {
	vp->v_cap.cap_port = vdport;
	if (prv_number(&vp->v_cap.cap_priv) == 0)   /* fill in capability */
	{
	    uniqport(&vp->v_cap.cap_priv.prv_random);
	    (void) prv_encode(&vp->v_cap.cap_priv, nextnum, PRV_ALL_RIGHTS,
					    &vp->v_cap.cap_priv.prv_random);
	    nextnum++;
	}
    
	/* Adjust physical partition labels */
	for (vpp = vp->v_parts, i = 0; i < vp->v_many; vpp++, i++)
	{
	    if (vpp == 0)
	    {
		printf("WARNING: corrupt virtual disk\n");
		printf("Don't know how to recover\n");
	    }
	    else
	    {
		if (vpp->i_physpart == 0)
		{
		    vpart *	xpp;
		    int		j;

		    /* We are missing a physical partition!
		     * Someone might have deleted it or the disk it is on
		     * is no longer accessible
		     */
		    printf("WARNING: Part of vdisk %d is missing\n",
					    prv_number(&vp->v_cap.cap_priv));
		    if (confirm("Abort labeling process"))
			exit(-1);
		    printf("Deleting it from the virtual disk description.\n");
		    /* Go through and decrement v_many field in subpartition
		     * tables then recompute checksums.
		     */
		    vp->v_many--;
		    i--;
		    for (xpp = vp->v_parts, j = 0; j < vp->v_many; xpp++, j++)
		    {
			if (xpp->i_physpart != 0)
			    xpp->i_physpart->label.d_ptab[xpp->i_part].p_many = vp->v_many;
			xpp->i_physpart->label.d_chksum = 0;
			xpp->i_physpart->label.d_chksum =
				  checksum((uint16 *) &xpp->i_physpart->label);
		    }
		    vpp->i_numblks = 0;
		}
		else
		{
		    vpp->i_physpart->label.d_ptab[vpp->i_part].p_cap = vp->v_cap;
		}
	    }
	}
    }

    /* For good measure rewrite all the partition (amoeba) labels */
    for (plp = Partitions; plp != 0; plp = plp->next)
    {
	(void) memset(plp->label.d_filler, 0, FILLER_SIZE);
	err = write_amlabel(&plp->diskcap, plp->name, plp->labelblock,
								&plp->label);
	if (err != STD_OK)
	{
	    fprintf(stderr, "%s: label_vdisks failed\n", Prog);
	    exit(-1);
	}
    }

    if (Vdtab)
	free_vdisk_list(Vdtab);
    Vdtab = vdtab;
}


/*
 * renumber_vdisks
 *
 *	Go through all the virtuals disks and renumber them starting from 1.
 *	This simplifies working in some cases.
 *	For each virtual disk we give it a new number and update the
 *	partition tables in the physical disk labels.
 *	Then we rewrite the labels and remake the list of virtual disks.
 */

void
renumber_vdisks _ARGS(( void ))
{
    int		i;
    disklabel *	dp;
    private *	pp;
    part_list *	plp;
    vdisktab *	vp;
    vpart *	vptr;
    objnum	newnum;
    errstat	err;

    /* For each virtual disk we know about */
    for (newnum = 1, vp = Vdtab; vp != 0; newnum++, vp = vp->v_next)
    {
	/* For each vdisk partition we update the physical partition info */
	for (vptr = vp->v_parts, i = 0; i < vp->v_many; vptr++, i++)
	{
	    plp = vptr->i_physpart;
	    dp = &plp->label;
	    pp = &dp->d_ptab[vptr->i_part].p_cap.cap_priv;
	    (void) prv_encode(pp, newnum, PRV_ALL_RIGHTS, &pp->prv_random);
	    dp->d_chksum = 0;
	    dp->d_chksum = checksum((uint16 *) dp);

	    /* Rewrite Partition label */
	    err = write_amlabel(&plp->diskcap, plp->name, plp->labelblock, dp);
	    if (err != STD_OK)
	    {
		fprintf(stderr, "%s: renumber failed\n", Prog);
		exit(-10);
	    }
	}
    }
    make_vdisk_list();
}
