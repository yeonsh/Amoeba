/*	@(#)main.c	1.5	96/02/27 10:12:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	BULLET SERVER MAKE FILE SYSTEM (B_MKFS)
**
**	This utility runs in user mode on raw amoeba and simply writes an
**	bullet server superblock on block 0 of the specified virtual disk.
**	There are two optional parameters:
**	  disk block size:	Must be at least 512 bytes and is specified
**				as the log (base 2) of the size in bytes.
**				The minumum (which is also the default) is 9.
**	  number of inodes:	Default is #disk blocks / 10
**
**	The program finds out the disk size itself and works the rest out.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"
#include "module/name.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"

#include "stdio.h"
#include "stdlib.h"

errstat		b_mkfs();
errstat		check_disk();
int		getopt();
static int	log2();
static void	usage();

extern char *	optarg;
extern int	optind;

main(argc, argv)
int	argc;
char *	argv[];
{
    capability	cap;
    char *	diskname;
    char *	progname;
    int		blocksize;
    int		numinodes;
    errstat	status;
    int		maxblksz;
    int		opt;
    int		force;

    progname = argv[0];		/* name of this program */

/* calculate max blocksize we can use for the file system */
    maxblksz = log2(D_REQBUFSZ);

/*
** set defaults in case command line options aren't specified
** - if blocksize or numinodes is 0 b_mkfs uses its defaults
*/
    blocksize = 0;
    numinodes = 0;
    force = 0;

/* process arguments */
    while ((opt = getopt(argc, argv, "b:fi:")) != EOF)
	switch (opt)
	{
	case 'b':
	    blocksize = atoi(optarg);
	    if (blocksize < D_PHYS_SHIFT || blocksize > maxblksz)
	    {
		fprintf(stderr,
		    "Bad blocksize: %d (must be in the range [%d, %d])\n",
					    blocksize, D_PHYS_SHIFT, maxblksz);
		exit(-2);
		/*NOTREACHED*/
	    }
	    break;
	
	case 'f':
	    force = 1;
	    break;

	case 'i':
	    numinodes = atoi(optarg); 
	    break;

	default:
	    usage(progname);
	    /*NOTREACHED*/
	}

    if (optind != argc - 1)
    {
	usage(progname);
	/*NOTREACHED*/
    }
    diskname = argv[optind];

/* get capability for disk */
    if ((status = name_lookup(diskname, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					progname, diskname, err_why(status));
	exit(1);
	/*NOTREACHED*/
    }

/* make sure that there is not a bullet file system on the disk already */
    if (!force && (status = check_disk(&cap)) != STD_OK)
    {
	if (status == STD_EXISTS)
	{
	    fprintf(stderr, "%s: '%s' already contains a bullet file system.\n",
							progname, diskname);
	    fprintf(stderr, "Use -f option to overwrite it.\n");
	}
	else
	{
	    fprintf(stderr, "%s: check of partition '%s' failed: %s\n",
					progname, diskname, err_why(status));
	}
	exit(1);
	/*NOTREACHED*/
    }

/* make the file system */
    if ((status = b_mkfs(&cap, blocksize, numinodes)) != STD_OK)
    {
	fprintf(stderr, "%s: b_mkfs failed: %s\n", progname, err_why(status));
	exit(1);
	/*NOTREACHED*/
    }

    exit(0);
    /*NOTREACHED*/
}


static void
usage(name)
char *	name;
{
    fprintf(stderr,
	"Usage: %s [-f] [-b log2(blocksize)] [-i num_inodes] diskcap\n", name);
    exit(1);
}


/* 
** Calculate floor[log2(n)]
*/

static int
log2(n)
int	n;
{
    int	log;

    for (log = 0; (n >>= 1) > 0; log++)
	/* do nothing */ ;
    return log;
}
