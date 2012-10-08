/*	@(#)physlabel.c	1.5	96/02/27 10:12:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "string.h"
#include "stdlib.h"
#include "module/disk.h"
#include "disk/conf.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "proto.h"
#include "host_dep.h"

extern dsktab *		Pdtab;
extern int		Pdtabsize;
extern host_dep *	Host_p[];
extern int		Host_os;
/* Initialised in formatdata.c in this directory */
extern dsktype		Dtypetab[];
extern int		Dtypetabsize;


/*
 * ENQUIRE_TYPE
 *
 *	Find out the type string and fill in the label's type name field.
 */

static int
enquire_type(label)
disklabel *	label;
{
    dsktype *	dp;	/* pointer to disk type entry */
    int		i;	/* loop counter */
    int		type;	/* disk type */

    /* Print the known disk types */
    printf("Disk Types:\n");
    for (dp = Dtypetab, i = 1; i <= Dtypetabsize; dp++, i++)
	printf(" %d.  %s\n", i, dp->name);

    /* Ask the user to pick one */
    do
    {
	type = getnumber("Enter the type of the disk to be labeled: ") - 1;
	if (type >= Dtypetabsize || type < 0)
	    printf("Please give a number between 1 and %d\n", Dtypetabsize);
	else
	    printf("You chose: %s\n", Dtypetab[type].name);
    } while (type >= Dtypetabsize || type < 0 || !confirm("Are you sure"));

    /* If type was other then ask for a human readable name */
    if (strcmp("Other", Dtypetab[type].name) == 0)
	do
	{
	    printf("What type of disk is it [max %d characters]? ", TYPELEN);
	    getline(label->d_disktype, label->d_disktype+TYPELEN);
	    printf("Disk type is %s\n", label->d_disktype);
	} while (!confirm("Is that correct"));
    else
	(void) strcpy(label->d_disktype, Dtypetab[type].name);
    return type;
}


/*
 * LABELDISK
 *
 *	Actually does the work of puting the sticky label on the disk
 */

static void
labeldisk(disknum, dtab)
int		disknum;	/* number of disk to label */
dsktab *	dtab;		/* linked list of known disks */
{
    int		i;	/* loop counter */
    dsktab *	p;	/* pointer to table entry of disk to be labelled */
    int		type;	/* type of disk to be labelled. -1 if current label */
			/* should be modified. */

    /* Find pointer to table entry for selected disk */
    for (i = disknum, p = dtab; p != 0 && i != 0; i--)
	p = p->next;
    if (p == 0)
    {
	printf("Panic: Couldn't find disk %d in table.\n", disknum);
	return;
    }

    /* Check that disk isn't already labelled */
    type = 0;
    if (p->labelled == Host_os)
    {
	printf("\nThis disk already has a label.\n");
	printf("The current label is as follows:\n\n");
	(*Host_p[Host_os]->f_printlabel)(p->plabel);
	printf("\nChanging the label will almost certainly destroy any data on the disk.\n");
	if (!confirm("Are you sure that you want to change it"))
	    return;
	printf("\nYou can modify this label or make one from scratch.\n");
	if (confirm("Do you want to modify this label"))
	    type = -1;
    }
    if (type != -1)
	type = enquire_type(&p->amlabel);
    (*Host_p[Host_os]->f_labeldisk)(p, type);
}


/*
 * label_pdisks
 *
 *	This routine is used to label the physical disk(s) on the current host.
 *	It prints the list of known physical disks.
 *	It asks the user at the console to select the disk to be labelled.
 *	If the selected disk has a valid label it gets quite nervous but 
 *	can be persuaded to continue.
 *	The user must then specify the physical disk parameters.
 */

void
label_pdisks _ARGS(( void ))
{
    int		disknum;	/* number of the disk to label */
    int		try_count;	/* # consecutive failed attempts at an op. */

    /* Print list of disks available for labelling */
    display_pdisks();

    if (Pdtabsize > 1)
    {
	if (confirm("Do you wish to (re)label one of these disks"))
	    for (try_count = 0;;)
	    {
		/* Find out which disk to use */
		disknum = getnumber(
			"Please enter the number of the disk to be labeled : ");
		printf("\nYou selected %d\n", disknum);
		if (disknum < 0 || disknum >= Pdtabsize)
		{
		    printf("That is not one of the disks available.\n");
		    switch (try_count++)
		    {
		    case 0:
			printf("Please THINK before making your selection.\n");
			continue;

		    case 1:
			printf("I'll give you just one more chance!\n");
			continue;

		    default:
			printf("You don't seem to know what you are doing.\n");
			printf("Why don't you go and get some help? (RTFM?)\n");
			printf("Adios Luser.\n");
			exit(2);
			/*NOTREACHED*/
		    }
		}
		try_count = 0;	/* successful selection! */

		/* Label the disk */
		labeldisk(disknum, Pdtab);
		if (confirm("\nDo you wish to label another disk"))
		    display_pdisks();
		else
		    break;
	    }
    }
    else  /* only one disk to label */
	if (confirm("Do you wish to label this disk"))
	    labeldisk(0, Pdtab);
    
    /*
     * Now we remake the list of physical partitions since it probably changed
     */
    make_partn_list();
    return;
}
