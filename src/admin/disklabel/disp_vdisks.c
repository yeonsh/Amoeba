/*	@(#)disp_vdisks.c	1.5	96/02/27 10:12:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * disp_vdisks
 *
 *	Displays the virtual disk table.
 *
 * Author: Gregory J. Sharp, June 1992
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
#include "proto.h"

extern vdisktab	*	Vdtab;
extern char *		Hostname;

void
display_vdisks _ARGS(( void ))
{
    objnum	vdisknum;	/* number of the virtual disk found */
    int		i;		/* loop counter */
    vpart *	vpp;		/* pointer to virtual disk's partition table */
    vdisktab *	vtab;

    if ((vtab = Vdtab) == (vdisktab *) 0)
    {
	printf("\nThere are no virtual disks defined on host ``%s''\n", Hostname);
	return;
    }

    printf("\nVirtual disk table for host ``%s''\n", Hostname);
    for (vtab = Vdtab; vtab != 0; vtab = vtab->v_next)
    {
	vdisknum = prv_number(&vtab->v_cap.cap_priv);
	printf("\nVirtual Disk %ld: %ld sectors of %d bytes\n",
				    vdisknum, vtab->v_numblks, D_PHYSBLKSZ);
	for (vpp = vtab->v_parts, i = 0; i < vtab->v_many; vpp++, i++)
	{
	    if (vpp->i_numblks > 0)
	    {
		printf("disk %s, Partn %d, 1st Sect = %8ld, #Sects = %8ld\n",
			vpp->i_physpart->name, vpp->i_part,
			vpp->i_firstblk - vpp->i_physpart->firstblock,
			vpp->i_numblks);
	    }
	}
    }
}


void
menu_display_vdisks _ARGS(( void ))
{
    display_vdisks();
    pause();
}
