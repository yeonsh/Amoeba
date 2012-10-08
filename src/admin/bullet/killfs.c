/*	@(#)killfs.c	1.6	96/02/27 10:11:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	BULLET SERVER KILL FILE SYSTEM
**
**	This utility runs in user mode under unix or amoeba and annihilates
**	a bullet server superblock on the start of the specified virtual
**	disk.
**	This is needed since there should be only one bullet file system on
**	a disk sub-system since the bullet server can manage only one file
**	system and has to work out which virtual disk to use by inspecting
**	them all.
**	Because some people make mistakes it only overwrites the magic number
**	in the superblock.  This allows you to recover by restoring the
**	magic number later.  Killfs -r will do this.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "byteorder.h"
#include "module/disk.h"
#include "module/name.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"

#include "stdio.h"
#include "stdlib.h"

extern uint16	checksum();

int
valid_superblk(super)
Superblk *	super;
{
    uint16	sum;

    /* Calculate superblock checksum */
    sum = checksum((uint16 *) super);

    dec_l_be(&super->s_magic);
    if (super->s_magic != S_MAGIC || sum != 0)
	return 0;
    return 1;
}


main(argc, argv)
int	argc;
char *	argv[];
{
    Superblk	super;
    capability	cap;
    char *	diskname;
    char *	progname;
    errstat	status;
    int		rflag;

    progname = argv[0];
    rflag = 0;
    switch (argc)
    {
    case 2:
	break;
    case 3:
	if (strcmp("-r", argv[1]) == 0)
	{
	    rflag = 1;
	    break;
	}
	/* fall through! */
    default:
	fprintf(stderr, "Usage: %s [-r] diskcap\n", progname);
	exit(1);
	/*NOTREACHED*/
    }
    diskname = argv[argc - 1];
    

    /* get capability for disk */
    if ((status = name_lookup(diskname, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					progname, diskname, err_why(status));
	exit(1);
	/*NOTREACHED*/
    }

    /* get old superblock from disk */
    if ((status = disk_read(&cap, D_PHYS_SHIFT, (disk_addr) 0,
				(disk_addr) 1, (char *) &super)) != STD_OK)
    {
	fprintf(stderr, "%s: disk_read failed: %s\n", 
						    progname, err_why(status));
	exit(1);
	/*NOTREACHED*/
    }

    if (rflag)	/* then insert a valid magic number! */
    {
	/*
	 * If it was a file system killed using killfs then this will
	 * restore it.  Otherwise some handwork will be needed.
	 */
	super.s_magic = S_MAGIC;
	enc_l_be(&super.s_magic);
	if (!valid_superblk(&super))
	{
	    fprintf(stderr, "%s: %s doesn't look like a bullet file system\n",
							progname, diskname);
	    exit(1);
	    /*NOTREACHED*/
	}
    }
    else
    {
	if (!valid_superblk(&super))
	{
	    fprintf(stderr, "%s: %s doesn't look like a bullet file system\n",
							progname, diskname);
	    exit(1);
	    /*NOTREACHED*/
	}
	super.s_magic = 0;	/* destroy the file system */
    }

    /* write back the superblock */
    if ((status = disk_write(&cap, D_PHYS_SHIFT, (disk_addr) 0,
				(disk_addr) 1, (char *) &super)) != STD_OK)
    {
	fprintf(stderr, "%s: disk_write failed: %s\n",
						    progname, err_why(status));
	exit(1);
	/*NOTREACHED*/
    }

    exit(0);
    /*NOTREACHED*/
}
