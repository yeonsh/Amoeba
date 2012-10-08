/*	@(#)rpc_group.c	1.1	93/10/07 15:22:28 */
/*
 *
 * Replicated RPC Package
 * Copyright 1992 Mark D. Wood
 * Vrije Universiteit, The Netherlands
 *
 * Previous versions
 * Copyright 1990, 1991 by Mark D. Wood and the Meta Project, Cornell University
 *
 * Rights to use this source in unmodified form granted for all
 * commercial and research uses.  Rights to develop derivative
 * versions reserved by the author(s).
 *
 * group routines
 */

#define ISSOURCE


#include <stdio.h>
#include <stdlib.h>
#include <stdcom.h>
#include "rrpc/msg_rpc.h"
#include "rrpc_protos.h"


void msg_define_grp_entry(gp, entry, func, entry_name)
     GROUP *gp;
     int entry;
     void (*func) _ARGS((message *));
     char *entry_name;

/* Defines this entry number for handling messages.

   Entries are somewhat like the ISIS entries.
   The thread msg_grp_listener continually listens for messages to arrive.

   Basic operation:  marks this procedure as being the handler for the messages
   coming in with the specified entry point. The entry number must be in the
   range of 0 ... (MAX_MSG_ENTRIES - 1).

 */

{
    if ((entry < 0) || (entry >= MAX_MSG_ENTRIES))
      msg_fatal_error("msg_define_grp_entry, bad entry number");

    gp->grp_entry_table[entry].func = func;
    gp->grp_entry_table[entry].name = entry_name;

    if (!func)
      msg_fatal_error("msg_define_grp_entry, null func");
}



void msg_grp_listener(gp)
GROUP *gp;

/* never returns.... This routine listens for group messages on the Meta port 
   for the given group, handing incoming messages off to the appropriate
   routine.

   This routine runs as a separate thread, with one thread being created
   for each group that the stub handles. (More threads are temporarily created
   during a state transfer.)

   This routine also handles forwarded requests and replies.
   A forwarded request results in the appropriate ctx_entry being called;
   in order to handle forwarded requests, a pointer to the original message
   that triggered the request is tacked on to the message.  This pointer is
   only valid for the process that received the original Amoeba RPC.

   A forwarded reply is handled by adding the message to the queue of received
   replies.  NB: we assume that all replicas make the same sequence of RPC
   requests!  A forwarded reply received by the process that sent the reply is
   dropped, since that process has already passed the reply on up.  (We can
   safely do this.   It may happen that the message will
   be delivered locally, but not to anyone else (if delivered to any other
   survivors, then it is received by all survivors), because of a failure.
   However, the new coordinator will repeat the RPC, get the same reply, and
   forward it on to everyone.
 */

{
    message *mp;
    int entry;

    while (1) {
	mp = msg_grp_receive(gp);
	entry = MSG_ENTRY(mp);

	if (MSG_OPTIONS(mp) & MSG_CTXT_RMV) {
	    _replicated_context_remove(mp);
	} else if (MSG_OPTIONS(mp) & MSG_RPLY_FORWARD) {
	    if (MSG_GRP_SENDER_ID(mp) != gp->grp_state.st_myid) {
		/* if we were the sender, then just ignore message */
		extern message *_queued_reply_list;
		extern SIGNAL _reply_or_mchange;

		msg_increfcount(mp);
		msg_add_to_queue(&_queued_reply_list, mp);
		_sig_signal(&_reply_or_mchange);
	    }

	} else if (MSG_OPTIONS(mp) & MSG_REQ_FORWARD) {
	    extern struct msg_entry ctx_entry_table[MAX_MSG_ENTRIES];

	    if (!buf_get_long(mp->data+MSG_OFFSET_FORWARD, MSG_END(mp),
			      (long *) &mp->grp_forwarded_mp))
	      msg_fatal_error("msg_grp_listener, buf_get_long");

	    if (ctx_entry_table[entry].check_seqnum && _check_seqnum(mp)) {
		/* duplicate, so reply with last. If not original
		   recipient of RPC request, then just ignore.
		   See comments about duplicates in msg_listener */

		if (MSG_GRP_SENDER_ID(mp) == gp->grp_state.st_myid) {

		    if (mp->ctxtp->last_replied_seqnum < MSG_SEQNUM(mp)) {
			if (MSG_SEQNUM(mp)-mp->ctxtp->last_replied_seqnum != 1)
			  msg_fatal_error("MSG_SEQNUM-last_repld_seqnum!=1");
			_sig_wait(&mp->ctxtp->new_reply);
			if (MSG_SEQNUM(mp) != mp->ctxtp->last_replied_seqnum)
			  msg_fatal_error("MSG_SEQNUM!=last_repld_seqnum");
		    }

#ifdef DEBUG
		    if (msg_trace & TRACE_MSGS) {
			printf("grp_listener duplicate, replying with 0x%x:\n",
			       mp->ctxtp->last_reply);
			msg_printaccess(mp->ctxtp->last_reply);
		    }
#endif
		    mp->grp_forwarded_mp->grp_reply_mp = mp->ctxtp->last_reply;
		    msg_increfcount(mp->grp_forwarded_mp->grp_reply_mp);
		    sema_up(&mp->grp_forwarded_mp->grp_sync);
		}
	    } else {
		/* call the appropriate entry */
		message *reply;

		reply = (*ctx_entry_table[entry].func)(mp);
		msg_rpc_reply(mp, reply);
		if (reply)
		  msg_delete(reply);
	    }
	} else {
	    if (gp->grp_entry_table[entry].func) {

#ifdef DEBUG
		if (msg_trace & TRACE_MSGS)
		  printf("calling entry %s\n",
			 gp->grp_entry_table[entry].name);
#endif

		(*gp->grp_entry_table[entry].func)(mp);
	    } else {
		msg_fatal_error("msg_listener, NULL entry func");
	    }
	}
	msg_delete(mp);
    }
}



void msg_grp_forward(gp, mp)
     GROUP *gp;
     message *mp;

/* Forwards message to the group gp */

{
    extern int _my_rep_grpid;
    extern int _multiple_replicas;
    int old_options = MSG_OPTIONS(mp);
    char *old_end_ptr = mp->end_ptr;

/*    printf("%d forwarding message to group %s %d, size %d options 0x%x\n",
	   _my_rep_grpid, ar_port(&gp->grp_port),
	   _multiple_replicas, MSG_SIZE(mp),
	   MSG_OPTIONS(mp));  *** */

    /* store in message the pointer to the message.  When all members of 
       the group receive message, we can tell if the message was sent by this
       process by looking at the group id.  If and only if we were the original
       sender do we do the necessary putrep. */

    if (!buf_put_long(mp->data + MSG_OFFSET_FORWARD,
		      MSG_BUF_END(mp),
		      (long) mp))
      msg_fatal_error("msg_grp_forwarded, buf_put_long");

    old_options = MSG_OPTIONS(mp);
    MSG_OPTIONS(mp) |= MSG_REQ_FORWARD;
    sema_init(&mp->grp_sync, 0);

#ifdef DEBUG
    if (msg_trace & TRACE_MSGS) {
	printf("%d forwarding message to group %s %d, size %d\n",
	       _my_rep_grpid,
	       ar_port(&gp->grp_port), _multiple_replicas, MSG_SIZE(mp));
    }
#endif

    msg_grp_send(gp, MSG_ENTRY(mp), mp);

    mp->end_ptr = old_end_ptr;
    MSG_OPTIONS(mp) = old_options;

    /* wait for reply to be generated */

    sema_down(&mp->grp_sync);
    putrep(&mp->grp_reply_mp->hdr, mp->grp_reply_mp->data,
	    MSG_SIZE(mp->grp_reply_mp));
    msg_delete(mp->grp_reply_mp);

#ifdef DEBUG
    if (msg_trace & TRACE_MSGS)
      printf("finished forwarding message and replying\n");
#endif

}



char *msg_grp_entry_name(gp, entry)
     GROUP *gp;
     int entry;

{
    return gp->grp_entry_table[entry].name;
}
