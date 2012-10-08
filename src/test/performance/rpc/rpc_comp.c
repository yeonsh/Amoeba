/*	@(#)rpc_comp.c	1.3	96/02/27 10:56:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "amoeba.h"
#include "stderr.h"
#include "module/rpc.h"


port prt1,prt2;
header hdr1,hdr2;
char *buf;
header rhdr1,rhdr2;
char *rep1, *rep2;

char *prog = "rpc_comp";

main(argc, argv)
char **argv;
{
	long size, n, cnt, rval;
	long start, finish;
	register i;

	prog = argv[0];
	if (argc != 5) {
		fprintf(stderr, "Usage: %s port1 port2 size cnt\n", prog);
		return(1);
	}
	strncpy((char *) &prt1, argv[1], PORTSIZE);
	strncpy((char *) &prt2, argv[2], PORTSIZE);
	priv2pub(&prt1, &prt1);
	priv2pub(&prt2, &prt2);

	size = hdr1.h_size = hdr2.h_size = atoi(argv[3]);
	if (size < 0) {
		fprintf(stderr, "%s: size should be greater than 0\n", prog);
		exit(1);
	}
	cnt = atoi(argv[4]);
	buf = (char *) malloc((size_t) size);
	rep1 = (char *) malloc((size_t) (size + 1));
	rep2 = (char *) malloc((size_t) (size + 1));
	if (buf == NULL || rep1 == NULL || rep2 == NULL) {
		fprintf(stderr, "%s: could not allocate trans buffer\n", prog);
		exit(1);
	}
	time(&start);
	for(n=0;n<cnt;n++) {
		for (i=0;i<size;i++)
			buf[i] = rand()>>4;
		hdr1.h_command = hdr2.h_command = rand()%3 +1;
		hdr1.h_port = prt1;
		hdr2.h_port = prt2;
		rval = rpc_trans(&hdr1, buf, size, &rhdr1, rep1, size + 1);
		if (rval != size) {
			fprintf(stderr, "%s: rpc_trans failed with server 1 (%d:%s)\n",
			        prog, ERR_CONVERT(rval),
				err_why((errstat) ERR_CONVERT(rval)));
			return(1);
		}
		rval = rpc_trans(&hdr2, buf, size, &rhdr2, rep2, size + 1);
		if (rval != size) {
			fprintf(stderr, "%s: rpc_trans failed with server 2 (%d:%s)\n",
				prog, ERR_CONVERT(rval),
				err_why((errstat) ERR_CONVERT(rval)));
			return(1);
		}
		for (i=0;i<size;i++) {
			if (rep1[i]!=rep2[i]) {
				printf("in transaction %d, a difference at byte %d\n",n,i);
				break;
			}
		}
	}
	time(&finish);
	finish -= start;
	printf("transfered %ld bytes in %ld seconds\n", (long) size * cnt * 4, finish);
	return 0;
}
