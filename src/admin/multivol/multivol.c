/*	@(#)multivol.c	1.4	96/02/27 10:17:10 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * multivol.c
 *
 * Create multi volume (floppy disk) dumps. This program collects data from
 * standard input and writes it out in media-size chunks onto a disk device.
 * Each chunk is preceded by a volume header describing the volume name,
 * current, and next volume numbers. The volume header is written out in
 * little endian byte order so that dumps are portable across different
 * machine architectures.
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

char *volname = VL_NAME;	/* default volume name */
long volume = 0;		/* current volume */
long mediasize = 0;		/* media size (in blocks) */
int quiet = 0;			/* quiet option */

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
    while ((opt = getopt(argc, argv, "qb:n:s:")) != EOF) {
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
    if ((argc - optind) != 2) usage();

    /* allocate enough buffer space */
    if ((buffer = malloc((size_t) bufblks * D_PHYSBLKSZ)) == NULL) {
	perror("malloc");
	exit(1);
    }

    /* in theory every medium which has more than 1 block can be used */
    if ((mediasize = atol(argv[optind])) <= 1) {
	fprintf(stderr, "%s: bad media size\n", program);
	exit(1);
    }

    /* lookup the disk capability */
    if ((err = name_lookup(argv[optind + 1], &diskcap)) != STD_OK) {
	fprintf(stderr, "%s: can't find %s (%s)\n",
	    program, argv[optind + 1], err_why(err));
	exit(1);
    }

    /* we need tty for the <cr> input */
    if (!cinit()) {
	fprintf(stderr, "%s: no controlling tty\n", program);
	exit(1);
    }

    /*
     * Read media size bytes from standard in and copy it onto the volume
     */
    for (;;) {
	union vollabel vollabel;
	disk_addr offset = 1;
	unsigned short crc = (unsigned short) -1;
	/* compute CCITT CRC polynomial */

	/* wait for user to enter media */
	if (!quiet) cprintf("\7\7\7");
	cprintf("Enter volume %s-%ld, and press <cr> when ready ...",
	    volname, volume);
	(void) cgetline();

	/* setup volume label (make up some sensible defaults) */
	vollabel.vl_magic = VL_MAGIC;
	vollabel.vl_crc = 0;
	vollabel.vl_curvol = volume++;
	vollabel.vl_nextvol = -1;
	vollabel.vl_mediasize = mediasize;
	vollabel.vl_datasize = mediasize - 1;
	enc_l_le(&vollabel.vl_magic);
	enc_s_le(&vollabel.vl_crc);
	enc_l_le(&vollabel.vl_curvol);
	enc_l_le(&vollabel.vl_nextvol);
	enc_l_le(&vollabel.vl_mediasize);
	enc_l_le(&vollabel.vl_datasize);
	strncpy(vollabel.vl_name, volname, sizeof(vollabel.vl_name)-1);

	/* copy media size block, speed is our only worry */
        while (offset < mediasize) {
	    register disk_addr b = MIN(bufblks, mediasize - offset);

	    /* beware: # of blocks becomes # of actually read blocks */
	    if ((b = fread(buffer, D_PHYSBLKSZ, b, stdin)) <= 0) {
		/* end of standard input stream */
		vollabel.vl_crc = crc;
		enc_s_le(&vollabel.vl_crc);
		vollabel.vl_datasize = offset - 1;
		enc_l_le(&vollabel.vl_datasize);
		err = disk_write(&diskcap, D_PHYS_SHIFT,
			(disk_addr)0, (disk_addr)1, (bufptr) &vollabel);
		if (err != STD_OK)
		    fprintf(stderr, "%s: disk write failed (%s)\n",
			program, err_why(err));
		exit(err == STD_OK ? 0 : 1);
	    }

	    err = disk_write(&diskcap, D_PHYS_SHIFT, offset, b, buffer);
	    if (err != STD_OK) {
		fprintf(stderr, "%s: disk write failed (%s)\n",
		    program, err_why(err));
		exit(1);
	    }
	    crc = volcrc(crc, buffer, b << D_PHYS_SHIFT);
	    offset += b;
	}

	/* patch up the volume label and write it */
	vollabel.vl_crc = crc;
	enc_s_le(&vollabel.vl_crc);
	vollabel.vl_nextvol = volume;
	enc_l_le(&vollabel.vl_nextvol);
	err = disk_write(&diskcap, D_PHYS_SHIFT,
	    (disk_addr)0, (disk_addr)1, (bufptr) &vollabel);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: disk write failed (%s)\n",
		program, err_why(err));
	    exit(1);
	}
    }
    /* NOTREACHED */
}

void
usage()
{
    fprintf(stderr,
	"Usage: %s [-q] [-b n] [-s n] [-n name] media-size capability\n",
	program);
    exit(1);
}
