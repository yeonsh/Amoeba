/*	@(#)iboot.c	1.4	96/02/27 10:03:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "stdio.h"
#include "assert.h"
#include "byteorder.h"
#include "server/boot/bsvd.h"
#include "string.h"
#include "module/ar.h"
#include "module/name.h"
#include "module/disk.h"
#include "server/bullet/bullet.h"

#if !defined(UNIX_FILES)
#ifndef AMOEBA
extern errno;
#define UNIX_FILES	1
#include "sys/file.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/fcntl.h"
#else
#define UNIX_FILES	0
#endif /* unix */
#endif /* defined(UNIX_FILES) */


/*
 *	iboot: read/write the bootserver's virtual disk
 *
 *	To do:
 *	- Check that everything actually fits on the disk
 */

union {		/* Our block */
    struct bsvd_head head;
    char block[1 << L2BLOCK];
} data;
typedef int boolean;
#define TRUE 1

char *progname = "IBOOT";

boolean	/* Flags set by the commandline */
	bflag = 0,	/* Cap for binary */
	cflag = 0,	/* Config position */
	dflag = 0,	/* Data position */
	fflag = 0,	/* Config file */
	lflag = 0,	/* Write magic string (otherwise check it) */
	sflag = 0,	/* Bootsvr supercap */
	vflag = 0,	/* Verbose */
	wflag = 0;	/* Write flag */

void
usage()
{
    fprintf(stderr, "Usage: '%s [-v] [-writeflags] vdisk'\n", progname);
    if (!vflag)  fprintf(stderr, "More info: %s -v?\n", progname);
    else {
	fprintf(stderr, "Writeflags:\n");
	fprintf(stderr, "\t-b binary-cap\t-c conf-blk#\t-d data-blk#\n");
	fprintf(stderr, "\t-f config-file\t-l(abel)\t-s super-cap\n");
    }
    exit(2);
} /* usage */

long d_block1 = 1;			/* First block of data-file */
long c_block1 = 1;			/* First block of config-file */
capability bincap, super;		/* Capability for binary, getport */
capability diskcap;			/* Capability for disk */
char *diskname, *filename = NULL;	/* And their paths */
#if UNIX_FILES
int filehndl;
#else
capability filecap;
#endif

/*
 *	Argument parsing & testing for consistency
 *	Sets progname and the above variables or exits
 */
void
getargs(argc, argv)
    char *argv[];
{
    extern int getopt(), optind;
    extern long atol();
    extern char *optarg, *strrchr();
    errstat err;
    int opt;

    if (argv[0] != NULL) {
	progname = strrchr(argv[0], '/');
	if (progname == NULL) progname = argv[0];
	else ++progname;
    }

    while ((opt = getopt(argc, argv, "b:c:d:f:ls:v")) != EOF) switch (opt) {
    case 'b':
	wflag = bflag = TRUE;
	if ((err = name_lookup(optarg, &bincap)) != STD_OK) {
	    fprintf(stderr, "%s: cannot lookup %s\n", progname, optarg);
	    exit(1);
	}
	break;
    case 'c':
	wflag = cflag = TRUE;
	if ((c_block1 = atol(optarg)) < 1) {
	    fprintf(stderr, "config-block must be > 0\n");
	    usage();
	}
	break;
    case 'd':
	wflag = dflag = TRUE;
	if ((d_block1 = atol(optarg)) < 1) {
	    fprintf(stderr, "data-block must be > 0\n");
	    usage();
	}
	break;
    case 'f':
	wflag = fflag = TRUE;
#if UNIX_FILES
	if ((filehndl = open(filename = optarg, O_RDONLY, 0)) < 0) {
	    perror(filename);
	    exit(2);
	}
#else
	if ((err = name_lookup(filename = optarg, &filecap)) != STD_OK) {
	    fprintf(stderr, "%s: cannot lookup %s (%s)\n",
			progname, filename, err_why(err));
	    exit(2);
	}
#endif
	break;
    case 'l':
	wflag = lflag = TRUE;
	break;
    case 's':
	wflag = sflag = TRUE;
	if ((err = name_lookup(optarg, &super)) != STD_OK) {
	    fprintf(stderr, "%s: cannot lookup %s\n", progname, optarg);
	    exit(1);
	}
	break;
    case 'v':
	vflag = TRUE;
	break;
    default:
	usage();
    }
    argc -= optind;
    argv += optind;
    if (argc != 1) usage();

    if (lflag) {
	if (!fflag) {
	    fprintf(stderr, "%s: -l requires -f\n", progname);
	    exit(2);
	}
	if (!bflag) {
	    (void) memset(&bincap, 0, sizeof(capability));
	    fprintf(stderr, "Warning: using NULL-cap for -b\n");
	    bflag = TRUE;
	}
	if (!sflag) {
	    (void) memset(&super, 0, sizeof(capability));
	    fprintf(stderr, "Warning: using NULL-cap for -s\n");
	    sflag = TRUE;
	}
    }

    if (cflag && !fflag) {
	fprintf(stderr, "%s: option -c requires -f\n", progname);
	exit(2);
    }

    if ((err = name_lookup(diskname = argv[0], &diskcap)) != STD_OK) {
	fprintf(stderr, "%s: cannot lookup %s (%s)\n", 
			progname, diskname, err_why(err));
	exit(2);
    }
} /* getargs */

/*
 *	Read the zeroth block on the disk - exit if magic string incorrect
 */
void
readheader()
{
    errstat err;
    err = disk_read(&diskcap, L2BLOCK, 0, 1, data.block);
    if (err != STD_OK) {
	fprintf(stderr, "%s: could not disk_read %s (%s)\n",
			    progname, diskname, err_why(err));
	exit(1);
    }
    if (!lflag && strcmp(data.head.magic, BOOT_MAGIC)) {
	fprintf(stderr, "%s: %s has bad magic string.\n", progname, diskname);
	exit(1);
    }
    DEC_BSVDP(&data.head);
} /* readheader */

/*
 *	Write the zeroth block to disk
 */
void
writeheader()
{
    errstat err;
    strcpy(data.head.magic, BOOT_MAGIC);
    ENC_BSVDP(&data.head);
    err = disk_write(&diskcap, L2BLOCK, 0, 1, data.block);
    if (err != STD_OK) {
	fprintf(stderr, "%s: could not disk_write %s (%s)\n",
			    progname, diskname, err_why(err));
	exit(1);
    }
    DEC_BSVDP(&data.head);
} /* writeheader */

main(argc, argv)
    char *argv[];
{
    errstat err;
    long pos;			/* Conf-file position in bytes */
    long dblocks, fsize = -1;	/* Sizes of disk (blocks) and file (bytes) */

    getargs(argc, argv);

    /* Get sizes of file and disk: */
    if ((err = disk_size(&diskcap, L2BLOCK, &dblocks)) != STD_OK) {
	fprintf(stderr, "%s: %s is not a disk: %s\n",
			progname, diskname, err_why(err));
	exit(2);
    }

    if (fflag) {
#if UNIX_FILES
	struct stat st;
	if (fstat(filehndl, &st) < 0) {
	    perror(filename);
	    exit(2);
	}
	fsize = st.st_size;
#else
	if ((err = b_size(&filecap, &fsize)) != STD_OK) {
	    fprintf(stderr, "%s: '%s' doesn't know it's size (%s)\n",
		progname, filename, err_why(err));
	    exit(2);
	}
#endif /* UNIX_FILES */
    }

    if (vflag) {
	printf("disksize: %ld blocks of 2^%d = %ld bytes\n",
	    dblocks, L2BLOCK, dblocks * (1 << L2BLOCK));
	if (wflag) {
	    printf("filesize: %ld bytes\n", fsize);
	}
    }

    /* Read or make up the header */
    if (lflag) {
	/* Put c_block beyond (fixed-size) d_block */
	if (!dflag) d_block1 = 1;	/* Block 0 is the header */
	data.head.proccaps.block1 = d_block1;
	data.head.proccaps.size = PROCCAPSIZE;
	if (!cflag) c_block1 = d_block1 + (PROCCAPSIZE / BLOCKSIZE);
	data.head.conf.block1 = c_block1;
	assert(bflag && fflag && sflag);
	data.head.binary = bincap;
	data.head.conf.size = fsize;
	data.head.supercap = super;
    } else if (wflag) {
	readheader();
	if (bflag) data.head.binary = bincap;
	if (cflag) data.head.conf.block1 = c_block1;
	else c_block1 = data.head.conf.block1;
	if (dflag) data.head.proccaps.block1 = d_block1;
	else d_block1 = data.head.proccaps.block1;
	if (sflag) data.head.supercap = super;
	if (fflag) data.head.conf.size = fsize;
    } else {
	assert(!wflag);
	readheader();
	c_block1 = data.head.conf.block1;
	d_block1 = data.head.proccaps.block1;
    }

    if (vflag) {
	printf("Data-file: %6ld bytes, 1st block=%ld\n",
	    data.head.proccaps.size, data.head.proccaps.block1);
	printf("Conf-file: %6ld bytes, 1st block=%ld\n",
	    data.head.conf.size, data.head.conf.block1);
	printf("Binary: %s\n", ar_cap(&data.head.binary));
	printf("Supercap: %s\n", ar_cap(&data.head.supercap));
    }

    /* Test against overlap */
    if (c_block1 == d_block1) {
	fprintf(stderr, "%s: datablock starts at same block as configuration\n",
								progname);
	exit(1);
    } else if (d_block1 > c_block1) {
	fprintf(stderr, "%s: configuration should follow data\n", progname);
	exit(1);
    } else if (d_block1 + PROCCAPSIZE / BLOCKSIZE > c_block1) {
	fprintf(stderr, "%s: datablock ends over configuration\n", progname);
	exit(1);
    }

    pos = 0L;
    if (wflag) {
	if (vflag) printf("Writing the header\n");
	writeheader();
	/* Write the configuration */
	if (fflag) {
	    if (vflag) fprintf(stderr, "Writing the configuration\n");
	    assert(fsize >= 0);
	    while (pos < fsize) {
		long n_read;
#if UNIX_FILES
		n_read = read(filehndl, data.block, sizeof(data.block));
#else
		err = b_read(&filecap, pos, data.block,
			    sizeof(data.block), &n_read);
		if (err != STD_OK) {
		    fprintf(stderr, "%s: could not b_read %s (%s)\n",
				    progname, filename, err_why(err));
		    exit(1);
		}
#endif
		if (n_read != sizeof(data.block)) {	/* clear the rest */
		    (void) memset(data.block + n_read, 0,
					sizeof(data.block) - n_read);
		}
		pos += n_read;
		err = disk_write(&diskcap, L2BLOCK, c_block1++, 1, data.block);
		if (err != STD_OK) {
		    fprintf(stderr, "Could not write disk (%s)\n", err_why(err));
		    exit(1);
		}
	    }
	}
    } else {
	/* Read the file */
	long dcsize, blk;
	dcsize = data.head.conf.size;
	blk = data.head.conf.block1;
	while (pos < dcsize) {
	    err = disk_read(&diskcap, L2BLOCK, blk++, 1, data.block);
	    if (err != STD_OK) {
		fprintf(stderr, "%s: could not disk_read (%s)\n",
					progname, err_why(err));
		exit(1);
	    }
	    pos += sizeof(data.block);
	    printf("%.*s",
		pos < dcsize ? sizeof(data.block) :
		sizeof(data.block) - (pos - dcsize), data.block);
	}
    }
    exit(0);
} /* main */
