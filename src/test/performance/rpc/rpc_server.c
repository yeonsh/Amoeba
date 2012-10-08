/*	@(#)rpc_server.c	1.6	96/02/27 10:56:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "amoeba.h"
#include "module/rpc.h"
#include "module/mutex.h"
#ifdef AMOEBA
#include "exception.h"
#include "module/signals.h"
#include "thread.h"
#endif


port myport;
long bufsiz;
long servicetime;
char *cmd;
mutex mu_block;

long atol();

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

#ifdef AMOEBA
/*ARGSUSED*/
void
transsig(sig, us, extra)
signum sig;
thread_ustate *us;
_VOIDSTAR extra;
{
        printf("signal\n");
}
#endif


/*ARGSUSED*/
void
server(param, siz)
char *param;
int   siz;
{
	long size;
	header hdr;
	register char *buf;

	buf = (char *) malloc((size_t) bufsiz);
	if (buf == NULL) {
		fprintf(stderr, "%s: not enough memory\n", cmd);
		exit(1);
	}
#ifdef AMOEBA
	sig_catch((signum) SIG_TRANS, transsig, (_VOIDSTAR) NULL);
#endif
	do {
		hdr.h_port = myport;
		if ((size=rpc_getreq(&hdr, buf, bufsiz)) < 0) {
			fprintf(stderr, "%s: getreq failed (2)\n", cmd);
#ifdef AMOEBA
			thread_exit();
#else
			exit(1);
#endif
		}
		if(servicetime > 0) {
		    mu_trylock(&mu_block, servicetime);
		}
		if (hdr.h_command>0 && hdr.h_command<=NALGS) {
			(*alg[hdr.h_command-1])(buf,size);
			rpc_putrep(&hdr, buf, size);
		} else {
			/* note that h_offset rather than h_size is used */
			rpc_putrep(&hdr, buf, hdr.h_offset);
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
	if (argc != 4) {
		fprintf(stderr, "Usage: %s port bufsiz servicetime\n", cmd);
		return(1);
	}
	(void) strncpy((char *)&myport, argv[1], PORTSIZE);
	bufsiz = atol(argv[2]);
	servicetime = atol(argv[3]);
	mu_init(&mu_block);
	mu_lock(&mu_block);
	if (bufsiz < 0) {
		fprintf(stderr, "%s: invalid buffer size\n", cmd);
		exit(1);
	}
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
