/*	@(#)rrpc_server.c	1.3	96/02/27 10:57:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Performance measuring tool for Replicated RPC package of Mark Wood.
 * Used in conjunction with rrpc_client.c
 */

#include <stdio.h>
#include "rrpc/msg_rpc.h"
#include "thread.h"

message *server_func(mp)
message *mp;
{
    /* for benchmarking sake, we should always use same reply message. */

#if defined(DEBUG)
    extern int msg_numalloc, msg_numfree, msg_inuse;

    printf("%s: alloc %d free %d in use %d\n",
	   program_name, msg_numalloc, msg_numfree, msg_inuse);
#endif

    if (MSG_DATA_SIZE(mp)) {
	msg_increfcount(mp);
	return mp;
    } else {
	/* null data message */
	return NULL;
    }
}


/*ARGSUSED*/
main(argc, argv)
int argc;
char *argv[];
{
    extern int msg_trace;

    msg_trace = 0;
    program_name = argv[0];
    ctx_define_entry(1, server_func, "server_func");
    ctx_define_context("rrpc_server", TRUE, 0,
			(msg_state_xfer_routine_t *) NULL,
			(msg_state_rcv_routine_t *) NULL);
    thread_exit();
    /*NOTREACHED*/
}
