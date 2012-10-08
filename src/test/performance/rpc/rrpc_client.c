/*	@(#)rrpc_client.c	1.3	96/02/27 10:57:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Performance measuring tool for Replicated RPC package of Mark Wood.
 * Used in conjunction with rrpc_server.c
 */

#define DEBUG

#include "rrpc/msg_rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <module/syscall.h>
#include <thread.h>

#define MAX_SIZE	(MSG_MAXSIZE - 20)

long size, cnt;
char *prog;


void reaper()
{
    CTXT_NODE *sp = ctx_lookup_context("start");
    message *req_msg = msg_newmsg();
    int status = ctx_rpc(sp, 1, req_msg, (message **) NULL);
    if (status)
      printf("%s: reaper, ctx_rpc, status = %d\n", prog, status);
    msg_delete(req_msg);

    exit(0);
}



/*ARGSUSED*/
void loop(arg)
void *arg;
{
    char *buf;
    int i, status;
    int n;
    CTXT_NODE * svr;
    unsigned long start, finish;
    message *req_msg;

    extern int msg_numalloc;
    extern int msg_numfree;
    extern int msg_inuse;

    if(size > 0 && (buf = (char *) malloc((size_t) size)) == 0)
      fprintf(stderr, "%s: not enough memory available\n", prog);

    svr = ctx_lookup_context("rrpc_server");
    req_msg = msg_newmsg();
    n = cnt;

    /* do a single, NULL rpc to get the cache loaded */
    status = ctx_rpc(svr, 1, req_msg, (message **) NULL);
    if (status) {
	printf("%s: ctx_rpc, status = %d\n", prog, status);
	exit(-1);
    }

    if (size) {

	for (i=0;i<size;i++)
	  buf[i] = '*';
	buf[size-1] = (char) 0;
	msg_put_string(req_msg, buf);
	
	start = sys_milli();

	do {
	    message *reply_msg;

#ifdef DEBUG_MSG
	    printf("%s: alloc %d free %d in use %d\n",
		   prog, msg_numalloc, msg_numfree, msg_inuse);
#endif
	    status = ctx_rpc(svr, 1, req_msg, &reply_msg);
	    if (status) {
		printf("%s: ctx_rpc, status = %d\n", prog, status);
		exit(-1);
	    } else {
#ifdef DEBUG_MSG
		char *reply;

		msg_get_string(reply_msg, &reply);
		printf("%s: reply %s\n", prog, reply);
#endif
		msg_delete(reply_msg);
	    }
	} while (--n);
	
    } else {
	/* size = 0 */

	start = sys_milli();
	
	do {
#ifdef DEBUG_MSG
	    printf("%s: alloc %d free %d in use %d\n",
		   prog, msg_numalloc, msg_numfree, msg_inuse);
#endif

	    status = ctx_rpc(svr, 1, req_msg, (message **) NULL);
	    if (status) {
		printf("%s: ctx_rpc, status = %d\n", prog, status);
		exit(-1);
	    } else {
#ifdef DEBUG_MSG
		printf("%s: reply\n", prog);
#endif
	    }
	} while (--n);
    }

    finish = sys_milli();
    finish -= start;

    printf("%f milliseconds (%d x %d bytes in %d millisecs)\n",
	   (double) finish / cnt, cnt, size, finish); 

    msg_delete(req_msg);

    reaper();
}



/*ARGSUSED*/
message *client_start(mp)
message *mp;
{
    /* for benchmarking sake, should always use same reply message. */

    printf("%s: received start message\n", prog);

    _t_fork(loop, (void *) 0, "loop");

    return NULL;
}		



int main(argc, argv)
int argc;
char *argv[];
{
    extern int msg_trace;

    program_name =
      prog = argv[0];

    if (argc != 3) {
	fprintf(stderr, "Usage: %s size cnt\n", prog);
	return(1);
    }

    size = atoi(argv[1]);
    cnt = atoi(argv[2]);
    if (size > MAX_SIZE) {
	fprintf(stderr, "%s: max size is %d\n", prog, MAX_SIZE);
	return(1);
    }

    timeout((interval) 30000);

    msg_trace = TRACE_GRP | TRACE_REP;

    ctx_define_entry(1, client_start, "client_start");
    ctx_define_context("rrpc_client", TRUE, 0,
			(msg_state_xfer_routine_t *) NULL,
			(msg_state_rcv_routine_t *) NULL);
    thread_exit();

    return 0;
}
