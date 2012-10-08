/*	@(#)msg_server.c	1.3	96/02/27 10:56:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <unistd.h>
#include "rrpc/msg_rpc.h"
#include "thread.h"

extern int msg_numalloc;
extern int msg_numfree;
extern int msg_inuse;
extern int optind;

static int verbose = 0;
static unsigned int sleeptime = 0;
static char *progname = "msg_server";

message *server_func(mp)
message *mp;
{
    /* for benchmarking sake, should always use same reply message. */

    message *reply_msg;
    int req;

    if (verbose)
	printf("alloc %d free %d in use %d\n",
	       msg_numalloc, msg_numfree, msg_inuse); 

    reply_msg = msg_newmsg();
    if (msg_get_int(mp, &req))
	msg_fatal_error("server_func, msg_get_string");
    if (verbose)
	printf("received request %d\n", req);
    msg_put_int(reply_msg, req);
    if (sleeptime > 0)
	sleep(sleeptime);
    return reply_msg;
}

static void usage()
{
    fprintf(stderr, "usage: %s [-s] [-v]\n", progname);
    exit(1);
}

main(argc, argv)
int argc;
char *argv[];
{
    int opt;

    progname = argv[0];
    while ((opt = getopt(argc, argv, "sv?")) != EOF) {
	switch (opt) {
	case 's':
	    sleeptime++; break;
	case 'v':
	    verbose++; break;
	case '?':
	default:
	    usage(); break;
	}
    }
    if (optind != argc) {
	usage();
    }

    ctx_define_entry(1, server_func, "server_func");
    ctx_define_context("rpcserver", TRUE, 0,
			(msg_state_xfer_routine_t *) NULL,
			(msg_state_rcv_routine_t *)  NULL);
    thread_exit();
    /*NOTREACHED*/
}
