/*	@(#)pcomp.c	1.3	96/02/27 10:56:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "amoeba.h"

#define BUFSIZE		30000

port prt1, prt2;
header hdr1,hdr2;
char buf[BUFSIZE];
header rhdr1,rhdr2;
char rep1[BUFSIZE],rep2[BUFSIZE];

main(argc, argv)
char **argv;
{
	bufsize size, n, cnt;
	long start, finish;
	register bufsize i;

	if (argc != 5) {
		fprintf(stderr, "Usage: %s port1 port2 size cnt\n", argv[0]);
		return(1);
	}
	strncpy((char *) &prt1, argv[1], PORTSIZE);
	priv2pub(&prt1, &prt1);
	strncpy((char *) &prt2, argv[2], PORTSIZE);
	priv2pub(&prt2, &prt2);
	size = hdr1.h_size = hdr2.h_size = atoi(argv[3]);
	if (size > BUFSIZE) {
		fprintf(stderr, "bufsize should be in range 0..%d\n", BUFSIZE);
		exit(1);
	}
	cnt = atoi(argv[4]);
	time(&start);
	for(n=0;n<cnt;n++) {
		for (i=0;i<size;i++)
			buf[i] = rand()>>4;
		hdr1.h_command = hdr2.h_command = rand()%3 +1;
		hdr1.h_port = prt1;
		hdr2.h_port = prt2;
		if (trans(&hdr1, buf, size, &rhdr1, rep1, BUFSIZE) != size) {
			fprintf(stderr, "%s: trans failed with server 1\n", argv[0]);
			return(1);
		}
		if (trans(&hdr2, buf, size, &rhdr2, rep2, BUFSIZE) != size) {
			fprintf(stderr, "%s: trans failed with server 2\n", argv[0]);
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
