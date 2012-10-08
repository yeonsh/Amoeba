/*	@(#)rpc_client.c	1.4	96/02/27 10:56:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "amoeba.h"
#include "stderr.h"
#include "module/rpc.h"

extern int optind;
extern char *optarg;

#ifdef AMOEBA
#include "module/syscall.h"
#else

#include <sys/time.h>

unsigned long
sys_milli()
{
    struct timeval tp;

    if (gettimeofday(&tp, (struct timezone *) 0) != 0)
    {
	perror("gettimeofday");
	exit(2);
    }
    return (unsigned long) tp.tv_sec * 1000 + (unsigned long) tp.tv_usec / 1000;
}

#endif

header hdr;
char *buf;
char *prog = "rpc_client";

void usage()
{
	fprintf(stderr, "Usage: %s [-r] [-m nmeasure] port size cnt\n", prog);
	exit(1);
}

main(argc, argv)
int argc;
char **argv;
{
	int opt;
	long gsize, size;
	long start, finish, n, cnt;
	int reverse=0;
	errstat r;
	static port pub_port;
	int nmeasure = 0, nrpc = 0;
	long rpcstart, rpcstop;

	prog = argv[0];
	while ((opt = getopt(argc, argv, "rm:?")) != EOF) {
		switch (opt) {
		case 'r':
			reverse++;
			break;
		case 'm':
			nmeasure = atoi(optarg);
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	if (argc - optind != 3) {
		usage();
	}
	strncpy((char *) &pub_port, argv[optind], PORTSIZE);
	priv2pub(&pub_port, &pub_port);
	gsize = size = atoi(argv[optind + 1]);
	if(size > 0 && (buf = (char *) malloc((size_t) size)) == 0)
		fprintf(stderr, "%s: not enough memory available\n", prog);
	if (reverse) {
		/* h_size is only 16 bits in Amoeba5 so use h_offset instead */
		hdr.h_offset = size;
		size = 0;
	}
	cnt = atoi(argv[optind + 2]);
	if (cnt == 0) {
		hdr.h_extra = 1;
		n = 2;
	}
	else
		n = cnt;
	(void) timeout((interval) 10000);
	time(&start);
	do {
		if (nmeasure > 0 && nrpc == 0) {
		    rpcstart = sys_milli();
		}
		hdr.h_port = pub_port;
		if ((r = rpc_trans(&hdr, buf, size, &hdr, buf, gsize)) < 0) {
			fprintf(stderr, "%s: trans failed: %s\n", prog,
				err_why((errstat) r));
			return(1);
		}
		if (nmeasure > 0 && nrpc++ >= nmeasure) {
		    rpcstop = sys_milli();
		    printf("%s: %d rpc_trans calls took %d msec\n",
			   prog, nmeasure, rpcstop - rpcstart);
		    nrpc = 0;
		}
	} while (--n);
	if (cnt == 0)
		return 0;
	time(&finish);
	finish -= start;
	printf("transfered %ld bytes in %ld seconds\n",
		(unsigned long) gsize * cnt, finish);
	if (finish)
		printf("transfer rate = %ld bytes/second\n",
			(unsigned long) gsize * cnt / finish);
	printf("response time = %ld microsecs\n", 1000000 * finish / cnt);
	return 0;
}
