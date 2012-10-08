/*	@(#)msg_client.c	1.3	94/04/06 17:43:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <unistd.h>
#include "rrpc/msg_rpc.h"

static int counter;


message *xfer_client_state(seq)
int seq;
{
    message *msg; 

    if (seq)
      return NULL;

    msg = msg_newmsg();
    msg_put_int(msg, counter);
    return msg;
}



/*ARGSUSED*/
int rcv_client_state(seq, msg)
int seq;
message *msg;
{
    int v;

    v = msg_get_int(msg, &counter);
    if (v)
      fprintf(stderr, "Error in transfering state\n");
}

extern int msg_numalloc;
extern int msg_numfree;
extern int msg_inuse;
extern int optind;

static char *progname = "msg_client";

static void usage()
{
    fprintf(stderr, "usage: %s [-s] [-v]\n", progname);
    exit(1);
}

int main(argc, argv)
int argc;
char *argv[];
{
    message *req_msg;
    message *reply_msg;
    CTXT_NODE * svr;
    int reply;
    int status;
    msg_state_xfer_routine_t xfer_func[1];
    msg_state_rcv_routine_t rcv_func[1];
    int opt;
    unsigned int sleeptime = 0;
    int verbose = 0;
    int maxloop = 1000;

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

    if (optind < argc - 2) {
	usage();
    } else if (optind == argc - 1) {
	maxloop = atoi(argv[argc - 1]);
    }

    xfer_func[0] = xfer_client_state;
    rcv_func[0] = rcv_client_state;

    ctx_define_context("rpcclient", TRUE, 1, xfer_func, rcv_func);
    svr = ctx_lookup_context("rpcserver");

    /* We set reply to -1 since 1st valid reply# is 0 */
    reply = -1;
    for (counter = 0; counter < maxloop; counter++) {
	/* in doing benchmarking, would be ideal if same message can be used */

	if (sleeptime > 0)
	    sleep(sleeptime);

	if (verbose > 1)
	    printf("alloc %d free %d in use %d\n",
		   msg_numalloc, msg_numfree, msg_inuse);

	req_msg = msg_newmsg();
	msg_put_int(req_msg, counter);

	if (verbose)
	    printf("request: %d\n", counter);
	status = ctx_rpc(svr, 1, req_msg, &reply_msg);

	if (status != 0) {
	    printf("ctx_rpc, status = %d\n", status);
	    printf("Did you start Tmsg_server???\n");
	} else {
	    if (msg_get_int(reply_msg, &reply))
		msg_fatal_error("main, msg_get_string");
	    if (verbose)
		printf("reply: %d\n", reply);
	    msg_delete(reply_msg);
	}

	if (reply != counter) {
	    printf("reply != req (%d != %d)\n", reply, counter);
	    if (reply >= 0) {
		printf("seq num req %d seq num reply %d\n",
		       MSG_SEQNUM(req_msg), MSG_SEQNUM(reply_msg));
	    }

	    counter = reply;
	    sleep(5);
	}

	msg_delete(req_msg);
    }

    printf("test completed successfully after %d reliable RPCs\n", maxloop);
    exit(0);
    /*NOTREACHED*/
}
