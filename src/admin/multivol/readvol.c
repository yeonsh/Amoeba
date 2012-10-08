/*	@(#)readvol.c	1.4	96/02/27 10:17:15 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * readvol.c
 *
 * Read multi volume (floppy disk) dumps. This program collects all
 * data on the multple volume dumps and writes it as one stream to
 * standard output. Most of the code in this program deals with user
 * made errors and their recovery.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <amtools.h>
#include <byteorder.h>
#include <module/disk.h>
#include "vollabel.h"

#define	BUFFERBLKS 32
long bufblks = BUFFERBLKS;	/* buffer size */
char *buffer;			/* actual buffer */

char *volname = VL_NAME;	/* volume name */
long volume = 0;		/* current volume number */

int quiet = 0;			/* quiet option */
int showlabel = 0;		/* show volume label only */
char *program;			/* program name */

extern int optind;
extern char *optarg;

int cinit();
void usage();
void cprintf();
char *cgetline();
unsigned short volcrc();

main(argc, argv)
    int argc;
    char **argv;
{
    capability diskcap;
    errstat err;
    int opt;

    /* scan arguments */
    program = argv[0];
    while ((opt = getopt(argc, argv, "qb:ln:s:")) != EOF) {
	switch (opt) {
	case 'q': /* be quiet */
	    quiet++;
	    break;
	case 'b': /* buffer size */
	    if (!isdigit(*optarg) || (bufblks = atol(optarg)) < 0) {
		fprintf(stderr, "%s: bad buffer size\n", program);
		exit(1);
	    }
	    break;
	case 'n': /* volume name */
	    volname = optarg;
	    break;
	case 'l': /* show volume label */
	    showlabel = 1;
	    break;
	case 's': /* start at volume */
	    if (!isdigit(*optarg) || (volume = atol(optarg)) < 0) {
		fprintf(stderr, "%s: bad volume number\n", program);
		exit(1);
	    }
	    break;
	default:
	    usage();
	    break;
	}
    }
    if ((argc - optind) != 1) usage();

    /* allocate enough buffer space */
    if ((buffer = malloc((size_t) bufblks * D_PHYSBLKSZ)) == NULL) {
	perror("malloc");
	exit(1);
    }

    /* we need tty for output */
    if (!cinit()) {
	fprintf(stderr, "%s: no controlling tty\n", program);
	exit(1);
    }

    /* lookup the disk capability */
    if ((err = name_lookup(argv[optind], &diskcap)) != STD_OK) {
	fprintf(stderr, "%s: can't find %s (%s)\n",
	    program, argv[optind], err_why(err));
	exit(1);
    }

    /*
     * It is sometimes useful to show the label of an
     * (physically unmarked:-) medium.
     */
    if (showlabel) {
	union vollabel vollabel;

	if (!quiet) cprintf("\7\7\7");
	cprintf("Enter volume, and press return when ready ...");
	(void) cgetline();

	/* read and decode volume label */
	err = disk_read(&diskcap, D_PHYS_SHIFT,
		(disk_addr)0, (disk_addr)1, (bufptr) &vollabel);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: disk read failed (%s)\n",
		program, err_why(err));
	    exit(1);
	}
	dec_l_le(&vollabel.vl_magic);
	dec_s_le(&vollabel.vl_crc);
	dec_l_le(&vollabel.vl_curvol);
	dec_l_le(&vollabel.vl_nextvol);
	dec_l_le(&vollabel.vl_mediasize);
	dec_l_le(&vollabel.vl_datasize);
	if (vollabel.vl_magic != VL_MAGIC) {
	    fprintf(stderr, "%s: bad volume magic\n", program);
	    exit(1);
	}

	printf("Volume %s-%ld (next %ld): data %ld blks, media %ld blks (crc %04x)\n",
	    vollabel.vl_name, vollabel.vl_curvol, vollabel.vl_nextvol,
	    vollabel.vl_datasize, vollabel.vl_mediasize, vollabel.vl_crc);
	exit(0);
    }

    /*
     * Read every volume and write its contents to standard output.
     * Most of the work involved deals with preventing mistakes.
     */
    for (;;) {
	unsigned short crc = (unsigned short) -1;
	/* compute CCITT CRC polyunomial */
	union vollabel vollabel;
	disk_addr offset;

	for (;;) {
	    if (!quiet) cprintf("\7\7\7");
	    cprintf("Enter volume %s-%d, and press <cr> when ready ...",
		volname, volume);
	    (void) cgetline();

	    /* read and decode volume label */
	    err = disk_read(&diskcap, D_PHYS_SHIFT,
		    (disk_addr)0, (disk_addr)1, (bufptr) &vollabel);
	    if (err != STD_OK) {
		fprintf(stderr, "%s: disk read failed (%s)\n",
		    program, err_why(err));
		exit(1);
	    }
	    dec_l_le(&vollabel.vl_magic);
	    dec_s_le(&vollabel.vl_crc);
	    dec_l_le(&vollabel.vl_curvol);
	    dec_l_le(&vollabel.vl_nextvol);
	    dec_l_le(&vollabel.vl_mediasize);
	    dec_l_le(&vollabel.vl_datasize);

	    if (vollabel.vl_magic != VL_MAGIC) {
		cprintf("Bad volume magic.\n");
		continue;
	    }
	    if (strncmp(vollabel.vl_name, volname, sizeof(vollabel.vl_name)) ||
		vollabel.vl_curvol != volume) {
		cprintf("Unexpected volume %s-%d.\n",
		    vollabel.vl_name, vollabel.vl_curvol);
		continue;
	    }
	    break;
	}

	/* read 'vollabel.vl_datasize' blocks */
	for (offset = 1; offset <= vollabel.vl_datasize; offset += bufblks) {
	    disk_addr b = MIN(bufblks, vollabel.vl_datasize - offset + 1);

	    err = disk_read(&diskcap, D_PHYS_SHIFT, offset, b, buffer);
	    if (err != STD_OK) {
		fprintf(stderr, "%s: disk read failed (%s)\n",
		    program, err_why(err));
		exit(1);
	    }
	    crc = volcrc(crc, buffer, b << D_PHYS_SHIFT);
	    fwrite(buffer, D_PHYSBLKSZ, b, stdout);
	}
	if (vollabel.vl_crc != crc)
	    fprintf(stderr, "WARNING: wrong CRC check sum\n");

	/* a negative value denotes end of dump */
        if ((volume = vollabel.vl_nextvol) == -1)
	    exit(0);
    }
}

void
usage()
{
    fprintf(stderr,
	"Usage: %s [-q] [-l] [-b n] [-n name] [-s vol] capability\n", program);
    exit(1);
}
