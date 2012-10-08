/*	@(#)rrpc_start.c	1.3	96/02/27 10:57:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This program is used in conjunction with rrpc_server and rrpc_client
 * to make sure that all the replicas are created before they start making
 * performance measurements.
 */

#include <stdio.h>
#include "rrpc/msg_rpc.h"
#include "thread.h"


message *client_done(mp)
message *mp;
{
    /* for benchmarking sake, should always use same reply message. */

    printf("%s: received completion message\n", program_name);
    msg_rpc_reply(mp, (message *) NULL);
    exit(0);
}


/*ARGSUSED*/
int main(argc, argv)
int argc;
char *argv[];
{
    message *req_msg;
    CTXT_NODE * svr;
    int status = 0;

    program_name = argv[0];
    timeout((interval) 500);

    ctx_define_entry(1, client_done, "client_done");
    if (ctx_define_context("start", FALSE, 0,
			   (msg_state_xfer_routine_t *) NULL,
			   (msg_state_rcv_routine_t *) NULL) == NULL)
    {
	fprintf(stderr, "%s: ctx_define_context failed\n", program_name);
	exit(1);
    }

    timeout((interval) 5000);
    svr = ctx_lookup_context("rrpc_client");
    req_msg = msg_newmsg();

    do {
	status = ctx_rpc(svr, 1, req_msg, (message **) NULL);
	if (status) {
	    printf("%s: ctx_rpc: status = %d, ", program_name, status);
	    printf("addr_lookedup = %d, addr_isnull = %d\n",
		   svr->addr_lookedup,svr->is_null);
	}
    } while (status);

    msg_delete(req_msg);

    thread_exit();
    /*NOTREACHED*/
}
