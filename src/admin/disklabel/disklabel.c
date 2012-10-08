/*	@(#)disklabel.c	1.5	96/02/27 10:12:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * disklabel
 *
 *	Synopsis: disklabel hostname
 *
 *	This program is used to label Amoeba virtual disks and where necessary
 *	to put labels expected by bootproms onto disks.  (For example, Sun
 *	workstations expect a Sun-specific disklabel at block 0 of a disk.)
 *	This program can also be used to join various disk partitions into
 *	a single large virtual disk.  All the disks must be attached to the
 *	same host if they are to be merged.
 *
 *	The program begins by looking for all the "bootp:nn" disks attached
 *	to the specified kernel directory and then proceeds to offer the
 *	various labelling possibilities.  It does using menus.
 *
 * Author: Gregory J. Sharp, June 1992
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "string.h"
#include "module/host.h"
#include "stdio.h"
#include "stdlib.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "vdisk/vdisk.h"
#include "menu.h"
#include "proto.h"


char *		Prog;		/* name of this program as per argv[0] */
char *		Hostname;	/* Name of the host we labelling argv[1] */
int		Host_os;	/* selector for the Host base for the disk */
dsktab *	Pdtab;		/* Physical disk table */
int		Pdtabsize;	/* Physical disk table size */
vdisktab *	Vdtab;		/* Virtual disk table */
part_list *	Partitions;	/* list of physical partitions available */


static struct menu	main_menu[] = {
	{ "Exit disklabel program",	noop },
	{ "Display Physical Disks",	menu_display_pdisks },
	{ "Display Physical Partitions",display_partn_list },
	{ "Display Virtual Disks",	menu_display_vdisks },
	{ "Label Physical Disks",	label_pdisks },
	{ "Label Virtual Disks",	label_partns },
	{ "Concatenate Virtual Disks",	merge_vdisks },
	{ "Unmerge Virtual Disks",	unmerge_vdisks },
	{ 0,				0 }
};


extern struct menu	Host_os_menu[];


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat	err;
    capability	hostcap;
    int		item;

    Prog = argv[0];
    if (argc != 2)
    {
	fprintf(stderr, "Usage: %s hostname\n", Prog);
	exit(1);
	/*NOTREACHED*/
    }
    Hostname = argv[1];

    /* Get the capability of the specified hosts */
    if ((err = super_host_lookup(Hostname, &hostcap)) != STD_OK)
    {
	fprintf(stderr, "%s: cannot lookup '%s' (%s)\n",
					    Prog, Hostname, err_why(err));
	exit(1);
	/*NOTREACHED*/
    }

    /* Ask which type of operating system normally uses the disk */
    Host_os = execute_menu(
	"Which operating system disk label should be used?\n", Host_os_menu);

    /*
     * Find all the "bootp:nn" entries in the kernel directory
     * As side effect it builds the list of physical disks in Pdtab.
     */
    if (find_disks(&hostcap) <= 0)
    {
	printf("%s: no physical disks attached to host %s\n", Prog, Hostname);
	exit(0);
	/*NOTREACHED*/
    }
    /*
     * Now go through the physical disks available and make a list of physical
     * partitions in Partitions and the list of virtual disks in Vdtab.
     * These lists will have to be remade if any pdisk is repartitioned
     * by label_pdisk().  The Vdtab list must also be remade if a partition is
     * re-sub-partitioned using label_vdisk().
     */
    make_partn_list();
    make_vdisk_list();
    renumber_vdisks();

    /* Now let the lusers do their worst and try to label things.  */
    while ((item = execute_menu("", main_menu)) > 0)
	(*main_menu[item].m_func)();

    printf("%s: exiting\n", Prog);
    exit(0);
    /*NOTREACHED*/
}


/*
 * total_blocks
 *
 *	Returns the total number of blocks in use as given by the Amoeba
 *	partition table.
 */

disk_addr
total_blocks(ptab)
part_tab *	ptab;
{
    int 	i;
    disk_addr	sum;

    for (sum = 0, i = 0; i < MAX_PARTNS_PDISK; ptab++, i++)
	sum += ptab->p_numblks;
    return sum;
}
