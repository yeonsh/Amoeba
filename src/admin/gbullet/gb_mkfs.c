/*	@(#)gb_mkfs.c	1.1	96/02/27 10:01:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
/*
 * Author:
 * Modified:
 *	Ed Keizer        -  For group bullet file format
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
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"

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
    int		first;
    int		maxmembers;
    int		r_flag_seen ;
    int		num_replicas;
    int		safe_rep;
    int		maxblksz;
    int		opt;
    int		force;
    int		same_cap;

    progname = argv[0];		/* name of this program */

    first= 0 ;			/* Default initialization to */
				/* non-initial server */
    maxmembers=0 ;
    same_cap = 0 ;		/* Use different server/file port */

/* calculate max blocksize we can use for the file system */
    maxblksz = log2(D_REQBUFSZ);

/*
** set defaults in case command line options aren't specified
** - if blocksize or numinodes is 0 b_mkfs uses its defaults
*/
    blocksize = 0;
    r_flag_seen=0 ; num_replicas=1 ;
    numinodes = 0;
    force = 0;
    safe_rep=0 ;

/* process arguments */
    while ((opt = getopt(argc, argv, "Ssb:fi:r:m:1")) != EOF)
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

	case '1':
	    first = 1;
	    break;

	case 'S':
	    safe_rep = 1 ;
	    break;

	case 'r':
	    if ( r_flag_seen ) {
		fprintf(stderr,"%s: Double -r flag\n",progname) ;
		usage(progname);
	    }
	    r_flag_seen=1;
	    num_replicas = atoi(optarg); 
	    break;

	case 'm':
	    maxmembers = atoi(optarg); 
	    break;

	case 's':
	    same_cap=1 ;
	    break ;

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

    if ( r_flag_seen ) {
	if ( first ) {
	    if ( num_replicas<=0 || num_replicas>S_MAXMEMBER ) {
		fprintf(stderr,"%s: incorrect number of replicas (%d)\n",
			progname,num_replicas);
	    }
	} else {
	    fprintf(stderr,"%s: warning, -r option is only useful in combination with -1\n",
		    progname) ;
	}
    }

    if ( !first && maxmembers ) {
	fprintf(stderr,"%s: warning, -m option is only useful in combination with -1\n",
		progname) ;
    }

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
    if ((status = b_mkfs(&cap, blocksize, numinodes,
		first,maxmembers,num_replicas,safe_rep,same_cap)) != STD_OK)
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
	"Usage: %s [-f] [-b log2(blocksize)] [-i num_inodes] [-1] [-m max_members] [-r num_replicas] [-S] [-s] diskcap\n", name);
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
