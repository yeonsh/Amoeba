/*	@(#)msg_grp1.c	1.2	94/04/06 17:43:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include "rrpc/msg_rpc.h"
#include "semaphore.h"

#define COUNT	500

static int my_id;
static semaphore sem_ready;

void receive(mp)
message *mp;
{
    int ival;
    int idval;

    msg_get_int(mp, &idval);
    msg_get_int(mp, &ival);
    printf("%d: %d %d\n", my_id, idval, ival);
    if (idval == my_id && ival >= COUNT) {
	printf("received last message\n");
	sema_up(&sem_ready);
    }
}


main()
{
    GROUP *gp;
    int i;
    message *mp;

    sema_init(&sem_ready, 0);
    gp = msg_grp_join("msg_grp1", 0, 0,
		      (msg_state_xfer_routine_t *) NULL,
		      (msg_state_rcv_routine_t *) NULL,
		      (msg_grp_monitor_routine_t) NULL, (void *) NULL);
    my_id = GRP_MY_ID(gp);

    msg_define_grp_entry(gp, 1, receive, "receive");

    for (i = 0; i <= COUNT; i++) {
	mp = msg_newmsg();
	msg_put_int(mp, my_id);
	msg_put_int(mp, i);
	msg_grp_send(gp, 1, mp);
	msg_delete(mp);
    }

    sema_down(&sem_ready);
    printf("all done\n");
    msg_grp_leave(gp);
    exit(0);
    /*NOTREACHED*/
}
