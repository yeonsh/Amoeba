/*	@(#)find_disks.c	1.4	96/02/27 10:12:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FIND_DISKS
 *
 *	Routines to scan the specified kernel directory for physcial disks
 *
 * Author: Gregory J. Sharp, June 1992
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "sp_dir.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "server/soap/soap.h"
#include "module/disk.h"
#include "disk/conf.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "vdisk/vdisk.h"
#include "host_dep.h"


#define	BOOTP	"bootp:"

extern char *		Prog;     /* name of this program as per argv[0] */
extern host_dep *	Host_p[]; /* Table of routines for host os */
extern int		Host_psize;/* size of Host_p array */
extern int		Host_os;  /* Entry in Host_p to use */
extern dsktab *		Pdtab;	  /* Table of physical disks */
extern int		Pdtabsize;/* # physical disks in Pdtab */


/*
 * add_disk
 *	Adds the discovered disk to a list of known physical disks
 *	attached to the specified host.
 */

static void
add_disk(name, diskcap)
char *	name;
capability *	diskcap;
{
    char	label[D_PHYSBLKSZ];	/* holder of current disk label */
    disklabel	amlabel;
    dsktab *	newdsk;
    errstat	err;
    disk_addr	dksize;
    int		i;
    int		host;

    /* Eliminated potential garbage in label */
    (void) memset(label, 0, D_PHYSBLKSZ);

    /* Pick off the host label, if any! */
    if ((host = (*Host_p[Host_os]->f_getlabel)(diskcap, label, &amlabel)) >= 0)
    {
	if ((err = disk_size(diskcap, D_PHYS_SHIFT, &dksize)) != STD_OK)
	{
	    fprintf(stderr, "%s: cannot get size of disk %s (%s)\n",
						    Prog, name, err_why(err));
	    return;
	}
	/* Add device to our list */
	if ((newdsk = (dsktab *) malloc(sizeof (dsktab))) == 0)
	{
	    fprintf(stderr, "%s: cannot malloc dsktab struct\n", Prog);
	    exit(1);
	    /*NOTREACHED*/
	}
	newdsk->diskcap = *diskcap;
	newdsk->size = dksize;
	(void) strncpy(newdsk->name, name, DNAMELEN);
	newdsk->name[DNAMELEN-1] = '\0';
	(void) memmove(newdsk->plabel, label, D_PHYSBLKSZ);
	newdsk->amlabel = amlabel;
	/* Work out the label type - assume worst case until we know better */
	newdsk->labelled = NO_LABEL;
	/* Try most probable case first */
	if (host == 0)
	    newdsk->labelled = Host_os;
	else
	{
	    /* Does it perhaps have some other kind of label? */
	    for (i = 0; i < Host_psize; i++)
	    {
		if (i != Host_os && (*Host_p[i]->f_validlabel)(label))
		    newdsk->labelled = i;
	    }
	}
	/* Put it on the front of the list of disks */
	newdsk->next = Pdtab;
	Pdtab = newdsk;
	Pdtabsize++;
    }
}


/*
 * find_disks
 *	Returns the number of bootp disks found on the specified host.
 *	Returns -1 on error.  Builds a list of available disks.
 *	The soap routines are used directly because a name-based
 *	interface for sp_list is not available right now.
 */

int
find_disks(hostcap)
capability *	hostcap;
{
    errstat	err;
    capset	hostdir;
    capset	bootpcs;
    capability	bootp_cap;
    SP_DIR *	dirp;
    int		i;
    int		numdisks;
    struct sp_direct *	entryp;

    /* Make a capset from the hosts capability so we can use sp_list */
    if (cs_singleton(&hostdir, hostcap) == 0)
    {
	fprintf(stderr, "%s: cannot make a capability set\n", Prog);
	return -1;
    }

    /* get all the names of entries in the host's kernel directory */
    if ((err = sp_list(&hostdir, &dirp)) != STD_OK)
    {
	fprintf(stderr, "%s: cannot list host's kernel directory (%s)\n",
							Prog, err_why(err));
	return -1;
    }

    /* Find all the kernel directory entries that are for physical disks */
    numdisks = 0;
    for (entryp = dirp->dd_rows, i = dirp->dd_nrows; i > 0; entryp++, i--)
    {
	if (strncmp(entryp->d_name, BOOTP, sizeof (BOOTP) - 1) == 0)
	{
	    if ((err = sp_lookup(&hostdir, entryp->d_name, &bootpcs)) != STD_OK)
	    {
		fprintf(stderr, "%s: sp_lookup of %s failed (%s)\n",
					Prog, entryp->d_name, err_why(err));
		return -1;
	    }
	    if ((err = cs_to_cap(&bootpcs, &bootp_cap)) != STD_OK)
	    {
		fprintf(stderr, "%s: cs_to_cap failed (%s)\n",
							Prog, err_why(err));
		return -1;
	    }
	    numdisks++;
	    add_disk(entryp->d_name, &bootp_cap);
	}
    }
    return numdisks;
}
