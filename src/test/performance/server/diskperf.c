/*	@(#)diskperf.c	1.3	96/02/27 10:57:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Measure disk performance
 *
 * The performance of the disk is tested by writting 64 kilo bytes
 * on the disk and reading it. Every pass is repeated 100 times for
 * accuracy. To measure the impact of block sizes, block sizes of 512,
 * 1024, 2048, 4096, and 8192 are sampled.
 *
 * NOTE: for historical reasons, the granuality is in seconds.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <module/disk.h>
#include <module/name.h>

#define	MAXDUR	30			/* 30 seconds per test */
#define	NBYTES	(64 * 1024L)		/* 64 kilo byte */

long blocksizes[] = {
    512, 1024, 2048, 4096, 8192, 16384
};

extern char *optarg;
extern int optind;

void
usage(p)
char * p;
{
    fprintf(stderr, "Usage: %s [-w] [-d duration] [-b bytes] capability\n", p);
}


main(argc, argv)
    int argc;
    char *argv[];
{
    long nbytes;
    capability diskcap;
    int i, maxdur;
    errstat err;
    int opt;
    int wrok = 0;
    
    maxdur = MAXDUR;
    nbytes = NBYTES;

    while ((opt = getopt(argc, argv, "b:d:w")) != EOF) {
	switch (opt) {
	case 'b':	/* # of bytes */
	    nbytes = atol(optarg);
	    break;
	case 'd':	/* # max time spent in each test */
	    maxdur = atoi(optarg);
	    break;
	case 'w':
	    wrok = 1;
	    break;
	default:
	    usage(argv[0]);
	    exit(2);
	}
    }
    if (optind >= argc) {
	usage(argv[0]);
	exit(2);
    }

    if ((err = name_lookup(argv[optind], &diskcap)) != STD_OK) {
	fprintf(stderr, "%s: %s lookup failed (%s)\n",
	    argv[0], argv[optind], err_why(err));
	exit(1);
    }

    printf("Blocksize      Writes         Reads\n");
    printf(" (bytes)     (bytes/sec)    (bytes/sec)\n");
    for (i = 0; i < (sizeof(blocksizes)/sizeof(long)); i++) {
	long throughput, duration;
	int l2blksz;
	char *blkptr;
	int c;
	disk_addr b, nblks;
	long start, finish;

	nblks = nbytes / blocksizes[i];
	blkptr = (char *) malloc((unsigned)blocksizes[i]);
	if (blkptr == NULL) {
	    fprintf(stderr, "%s: not enough memory\n", argv[0]);
	    exit(1);
	}

	/* compute log2(blocksizes[i]) */
	for (l2blksz = 0; ((1<<l2blksz) & blocksizes[i]) == 0; l2blksz++)
	    /* nothing */;

	printf("%6ld", blocksizes[i]);
	fflush(stdout);

	if (wrok) {
	    /* measure writes */
	    for (duration = 0, c = 0; duration < maxdur; c++) {
		time(&start);
		for (b = 0; b < nblks; b++) {
		    err = disk_write(&diskcap, l2blksz, b, 1L, blkptr);
		    if (err != STD_OK) {
			fprintf(stderr, "%s: %s: disk write failed\n",
			     err_why(err), argv[0]);
			exit(1);
		    }
		}
		time(&finish);
		duration += finish - start;
	    }
	    throughput = duration ? ((c * nbytes) / duration) : 0;
	    printf("         %6ld", throughput);
	} else
	    printf("         %6c", '?');
	fflush(stdout);
	
	/* measure reads */
	for (duration = 0, c = 0; duration < maxdur; c++) {
	    time(&start);
	    for (b = 0; b < nblks; b++) {
		err = disk_read(&diskcap, l2blksz, b, 1L, blkptr);
		if (err != STD_OK) {
		    fprintf(stderr, "%s: %s: disk write failed\n",
			 err_why(err), argv[0]);
		    exit(1);
		}
	    }
	    time(&finish);
	    duration += finish - start;
	}
	throughput = duration ? ((c * nbytes) / duration) : 0;
	printf("         %6ld\n", throughput);
	fflush(stdout);

	free((_VOIDSTAR) blkptr);
    }
    exit(0);
    /*NOTREACHED*/
}
