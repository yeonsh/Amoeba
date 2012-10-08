/*	@(#)pserver.c	1.5	96/02/27 10:56:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "amoeba.h"
#ifdef AMOEBA
#include "thread.h"
#endif

#define BUFSIZE		30000

port myport;
char *cmd;

alg0() {}

alg1(buf,size)  char *buf; {
	register i;

	for (i=0;i<size;i++)
		buf[i]++;
}

alg2(buf,size) char *buf; {
	register i;

	for (i=0;i<size;i++) {
		if (buf[i]&1)
			buf[i] = 0;
		else
			buf[i] = "hello world"[i%11];
	}
}

int (*alg[])() = {
	alg0,
	alg1,
	alg2
};
#define NALGS 3

/*ARGSUSED*/
static void
server(param, siz)
char *param;
int   siz;
{
	bufsize size;
	header hdr;
	register char *buf;

	buf = (char *) malloc((size_t) BUFSIZE);
	if (buf == NULL) {
		fprintf(stderr, "%s: cannot allocate %d bytes\n", cmd, BUFSIZE);
		exit(1);
	}
	do {
		hdr.h_port = myport;
		if ((short) (size=getreq(&hdr, buf, BUFSIZE)) < 0) {
			fprintf(stderr, "%s: getreq failed (2)\n", cmd);
#ifdef AMOEBA
			thread_exit();
#else
			exit(1);
#endif
		}
		if (hdr.h_command>0 && hdr.h_command<=NALGS) {
			(*alg[hdr.h_command-1])(buf,size);
			putrep(&hdr, buf, size);
		} else {
			putrep(&hdr, buf, hdr.h_size);
		}
	} while (!hdr.h_extra);
#ifdef AMOEBA
	thread_exit();
#else
	exit(0);
#endif
}

main(argc, argv)
char **argv;
{
	cmd = argv[0];
	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", cmd);
		return(1);
	}
	strncpy((char *) &myport, argv[1], PORTSIZE);
#ifndef AMOEBA
	fork();
#else
	if (thread_newthread(server, 4096, (char *) 0, 0) == 0) {
		fprintf(stderr, "%s: thread_newthread failed\n", cmd);
		return(1);
	}
#endif
	server((char *) 0, 0);
	return(0);
}
