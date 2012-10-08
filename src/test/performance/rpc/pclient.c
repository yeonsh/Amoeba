/*	@(#)pclient.c	1.2	94/04/06 17:44:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "amoeba.h"
#include "stderr.h"

#define BUFSIZE		30000

header hdr;
char buf[BUFSIZE];

main(argc, argv)
char **argv;
{
	bufsize gsize, size;
	long start, finish, n, cnt;
	char *prog;
	int reverse=0;
	errstat r;
	static port pub_port;

	prog = argv[0];
	if(argc>1 && strcmp(argv[1],"-r")==0) {
		reverse++;
		argc--;
		argv++;
	}
	if (argc != 4) {
		fprintf(stderr, "Usage: %s [ -r ] port size cnt\n", prog);
		return(1);
	}
	strncpy((char *) &pub_port, argv[1], PORTSIZE);
	priv2pub(&pub_port, &pub_port);
	gsize = size = atoi(argv[2]);
	if (reverse) {
		hdr.h_size = size;
		size = 0;
	}
	cnt = atoi(argv[3]);
	if (cnt == 0) {
		hdr.h_extra = 1;
		n = 2;
	}
	else
		n = cnt;
	(void) timeout((interval) 5000);
	time(&start);
	do {
		hdr.h_port = pub_port;
		r = trans(&hdr, buf, size, &hdr, buf, sizeof(buf));
		if(ERR_STATUS(r)) {
                        fprintf(stderr, "%s: trans failed: %s (after %d RPCs)\n",
				argv[0], err_why((errstat) ERR_CONVERT(r)),
				(cnt > 0 ? cnt : 2) - n);
                        return(1);
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
