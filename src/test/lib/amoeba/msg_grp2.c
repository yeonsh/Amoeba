/*	@(#)msg_grp2.c	1.3	96/02/27 10:56:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include "rrpc/msg_rpc.h"
#include "module/ar.h"
#include "thread.h"

#define COUNT 500

static int my_id;

void receive(arg)
void *arg;
{
    GROUP *gp = (GROUP *) arg;
    int ival;
    int idval;
    float fval;
    char *sval;
    port pval;
    message *mp;
    int i;

    for (i = 0; i < COUNT; i++) {
	mp = msg_grp_receive(gp);
/*	msg_printaccess(mp); */
	if (msg_get_int(mp, &idval) < 0) exit(-1);
	if (msg_get_int(mp, &ival)) exit(-2);
	if (msg_get_real(mp, &fval)) exit(-3);
	if (msg_get_string(mp, &sval)) exit(-4);
	if (msg_get_port(mp, &pval)) exit(-5);
	msg_rewind(mp);
	msg_increfcount(mp);
	if (msg_get_int(mp, &idval)) exit(-7);
	msg_delete(mp);
	if (msg_get_int(mp, &ival)) exit(-9);
	msg_delete(mp);
	printf("%d: %d (%d) %d: %d %f %s %s\n",
	       my_id, i, msg_grp_num_members(gp),
	       idval, ival, fval, sval, ar_port(&pval));

	if ((msg_grp_num_members(gp) > 2) && (i < COUNT - 2)
	    && (GRP_MY_ID(gp) == 0)) {
	    i = COUNT - 2;
	}
    }
    msg_grp_leave(gp);
    exit(0);
}


main()
{
    int i;
    message *mp;
    GROUP *gp;
    port p;

    gp = msg_grp_join("msg_grp2", MSG_GRP_MANUAL_RCV | MSG_GRP_RESILIENT, 0,
		      (msg_state_xfer_routine_t *) NULL,
		      (msg_state_rcv_routine_t *) NULL,
		      (msg_grp_monitor_routine_t) NULL, (void *) NULL);
    my_id = GRP_MY_ID(gp);
    printf("my group id: %d\n", my_id);
    _t_fork(receive, gp, "receive");

    strncpy((char *) &p, "12345678", sizeof(port));

    for (i=0; i < COUNT; i++) {
	mp = msg_newmsg();
	msg_put_int(mp, my_id);
	msg_put_int(mp, i);
	msg_put_real(mp, 3.141);
	msg_put_string(mp, "moxy");
	msg_put_port(mp, &p);
	msg_grp_send(gp, 0, mp);
	msg_delete(mp);
    }

    thread_exit();
    /*NOTREACHED*/
}
