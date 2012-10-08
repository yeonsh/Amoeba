/*	@(#)group.c	1.3	96/02/27 11:04:25 */
/*
 *
 * Replicated Group package
 * Copyright 1992 Mark D. Wood
 * Vrije Universiteit, The Netherlands
 *
 * Rights to use this source in unmodified form granted for all
 * commercial and research uses.  Rights to develop derivative
 * versions reserved by the author.
 *
 */

#define ISSOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdcom.h>
#include <amoeba.h>
#include <module/proc.h>
#include <ampolicy.h>
#include <thread.h>
#include <rrpc/msg_rpc.h>
#include <module/prv.h>

#ifdef DEBUG
#include <module/ar.h>
#endif

#include <capset.h>
#include <server/soap/soap.h>


#define MAX_MSG_ENTRIES 16


int _multiple_replicas = FALSE;	/* more than one active replica? */
int _coordinator = 0;		/* am I the _coordinator for the group? */


/* State transfer routines.  Protocol for performing a join is as follows.
   The new member does the join, giving as the signature of the message an
   RPC put port.  The existing member(s) (active replication used here!)
   immediately upon receipt of the join message and before receipt of any new
   messages, send their state by doing a series of RPC's.  The new member does
   not do a grp_receive until it has received the transfered state.

   This protocol can potentially hang---if all existing members die before
   transfering their state.  A timeout could be used to recover from this. */


static void transfer_group_state(gp, join_req_msg)
GROUP *gp;
message *join_req_msg;

/* new member joining GROUP gp with associated join_req_msg */

{
    int status = 0;
    address *xfer_addr = MSG_SIGNATURE(join_req_msg);
    message *state_msg;
    int dom;

#ifdef DEBUG
    if (msg_trace & TRACE_GRP)
      printf("beginning state transfer, %d domains\n",
	     gp->grp_num_domains); 
#endif

    for (dom = 0; !status && (dom < gp->grp_num_domains); dom++) {
	int i = 0;
	do {
	    state_msg = (*gp->grp_state_xfer_routine[dom])(i);
	    i++;
	    if (state_msg) {
		MSG_SEQNUM(state_msg) = i;
		status = msg_rpc(xfer_addr, dom, state_msg, NULL);
		msg_delete(state_msg);
	    }
	} while (state_msg);
    }
    
    if (!status) {
	/* final end-of-transmission message. Useful so rcving RPC thread
	   will terminate */

	state_msg = msg_newmsg();
	msg_rpc(xfer_addr, MAX_XFER_DOMAINS, state_msg, NULL);
	msg_delete(state_msg);
    }

#ifdef DEBUG
    if (msg_trace & TRACE_GRP)
      printf("state transfer completed (RPC phase)\n");
#endif

}



static void receive_group_state(arg)
void *arg;

/* Routine forked off to receive the group state.  Repeatedly does RPC rcvs to
   get group state, signalling gp->grp_state_wait when all state has been
   received.  Thread currently persists, however, so that state transfers
   messages from other replicas are handled.  (The messages are ignored).
   The thread waits until it has received termination messages from all
   the other replicas active at the time it did the amoeba join.  If some
   of those replicas should fail, this thread will persist forever. */

{
    GROUP *gp = (GROUP *) arg;
    int dom_seqnum[MAX_XFER_DOMAINS];
    message *msg;
    int dom;
    int num_expected = GRP_NUM_MEMBERS(gp) - 1;


    (void) memset((_VOIDSTAR) dom_seqnum, 0, sizeof(dom_seqnum));

    while (num_expected) {
	msg = msg_rpc_rcv(&gp->grp_xfer_gaddr);
	msg_rpc_reply(msg, NULL);

	if (gp->grp_active)
	  continue;

	dom = MSG_ENTRY(msg);

#ifdef DEBUG
	if (msg_trace & TRACE_GRP)
	  printf("rcvd state msg dom %d seqnum %d\n",dom, MSG_SEQNUM(msg));
#endif

	if (dom >= MAX_XFER_DOMAINS) {
	    num_expected--;
	    msg_delete(msg);

#ifdef DEBUG
	    if (msg_trace & TRACE_GRP)
	      printf("state transfer completed (RPC phase)\n");
#endif

	    gp->grp_active = TRUE;

	    _sig_signal(&gp->grp_state_wait);
	    continue;
	}

	if (MSG_SEQNUM(msg) > dom_seqnum[dom]) {
	    dom_seqnum[dom]++;
	    if (MSG_SEQNUM(msg) != dom_seqnum[dom])
	      msg_fatal_error("receive_group_state, SEQNUM !=");
	    (*gp->grp_state_rcv_routine[dom])(MSG_SEQNUM(msg) - 1, msg);
	}
	msg_delete(msg);
    }

#ifdef DEBUG
	    if (msg_trace & TRACE_GRP)
	      printf("state transfer receive thread exiting\n");
#endif

    thread_exit();
}



static void group_member_change(gp, action, mp)
     GROUP *gp;
     int action;
     message *mp;

/* received word that a group reset or a group leave has been received
   Updates group structure and calls the monitoring routine.
 */

{
    int status;

    status = grp_info(gp->grp_handle, &gp->grp_port,
		      &gp->grp_state, gp->grp_mem_id, MAX_GROUP_SIZE);

    check_amoeba_error(status, "group_member_change, grp_info");

#ifdef DEBUG
    if (msg_trace & TRACE_GRP) {
	static
	  char *actions[5]={"JOIN_REQ","XFER_DONE","RESET","LEAVE","CREATE"};

	printf("group %s %s(%d): %s [%d], %d members\n",
	       gp->grp_name, ar_port(&gp->grp_port), gp->grp_handle,
	       actions[action - GRP_JOIN_REQ], action,
	       GRP_NUM_MEMBERS(gp));
    }
#endif

    _multiple_replicas = (GRP_NUM_MEMBERS(gp) > 1);
    _coordinator = I_AM_COORDINATOR(gp);

    switch (action) {
      case GRP_JOIN_REQ:
	if (gp->grp_num_domains) {
	    if (!mp) {		/* we are the one wanting to join */
		_t_fork(receive_group_state, (void*)gp, "receive_group_state");
		_sig_wait(&gp->grp_state_wait);
	    } else {
		/* We should be active here.  Transfer the group state to the
		   new member.  Note that no more grp_receives are done until
		   the state has been transfered. */

		if (!gp->grp_active)
		  msg_fatal_error("group_member_change, !active");
		transfer_group_state(gp, mp);
	    }
	} else {
	    /* no state to be transferred, so join is immediate */
	    gp->grp_active = TRUE;
	}
	if (gp->grp_monitor_routine)
	  (*gp->grp_monitor_routine)(gp, gp->grp_monitor_arg);
	break;

      case GRP_CREATE:
	gp->grp_active = TRUE;
	if (gp->grp_monitor_routine)
	  (*gp->grp_monitor_routine)(gp, gp->grp_monitor_arg);
	break;

      case GRP_LEAVE:
      case GRP_RESET:
	if (gp->grp_monitor_routine)
	  (*gp->grp_monitor_routine)(gp, gp->grp_monitor_arg);
	break;
    }
}


#define cs_isempty(cs) ((cs)->cs_final == 0)

static void
call_msg_grp_listener(arg)
void *arg;
{
    msg_grp_listener((GROUP *) arg);
}

GROUP *msg_grp_join(gname, options, num_domains,
		    grp_state_xfer_routine, grp_state_rcv_routine,
		    monitor_routine, arg)

     char *gname;
     int options;
     int num_domains;
     message *(*grp_state_xfer_routine[]) _ARGS((int));
     int (*grp_state_rcv_routine[]) _ARGS((int, message *));
     void (*monitor_routine) _ARGS((GROUP *, void *));
     void *arg;

{
    static int init_grp_dir = TRUE;
    static capability grp_dir_cap;

    header hdr;
    errstat status;
    capability grp_cap;
    capability *oldcap;
    capset gcapset;
    sp_entry dir_sp_entry;
    int joining_mode;


    GROUP *gp = (GROUP *) malloc(sizeof(GROUP));
    if (!gp)
      msg_fatal_error("msg_grp_join, malloc");

    if (num_domains > MAX_XFER_DOMAINS)
      msg_fatal_error("msg_grp_join, too many domains");

    /* fill in the group state stuff */

    gp->grp_name = _str_malloc(gname);
    gp->grp_monitor_routine = monitor_routine;
    gp->grp_monitor_arg = arg;
    gp->grp_active = FALSE;

    _sig_init(&gp->grp_state_wait);

    gp->grp_num_domains = num_domains;
    for ( ;num_domains--; ) {
	if (!grp_state_rcv_routine[num_domains] ||
	    !grp_state_xfer_routine[num_domains])
	  msg_fatal_error("msg_grp_join, null xfer/rcv domain func");

	gp->grp_state_rcv_routine[num_domains] =
	  grp_state_rcv_routine[num_domains];
	gp->grp_state_xfer_routine[num_domains] =
	  grp_state_xfer_routine[num_domains];
    }

    if (init_grp_dir) {
	/* First get the capability for the directory in which we register the
	   group capabilities. */

	status = name_lookup(RRPC_GRP_DIR, &grp_dir_cap);
	check_amoeba_error(status, "msg_grp_join, name_lookup failed");
	init_grp_dir = FALSE;
	/* We need a capset structure to call sp_install */
	if (!cs_singleton(&dir_sp_entry.se_capset, &grp_dir_cap))
	  msg_fatal_error("msg_grp_join, cs_singleton");
    }

    while (1) {

	status = sp_lookup(&dir_sp_entry.se_capset, gname, &gcapset);

	if (status || (cs_isempty(&gcapset))) {
	    if ((status == STD_NOTFOUND) || (cs_isempty(&gcapset))) {
		static long mask[3]  = {0xff,0,0};
		port check_field;

/*		printf("group does not exist, creating...\n"); *** */

		uniqport(&grp_cap.cap_port);
		uniqport(&check_field);
		if (prv_encode(&grp_cap.cap_priv, (objnum) 0,
			       PRV_ALL_RIGHTS, &check_field) != 0)
		  msg_fatal_error("msg_grp_join, prv_encode");

		if (!cs_singleton(&gcapset, &grp_cap))
		  msg_fatal_error("msg_grp_join, cs_singleton");

		if (status == STD_NOTFOUND) {

		    status = sp_append(&dir_sp_entry.se_capset, gname,
				       &gcapset, 3, mask);

		    /* There exists a (name,port) entry */

		    if (status) {
			if (status == STD_EXISTS) {
			    /* Another process has successfully installed a 
			       port in the interval after we determined that no
			       capability was present and when we tried to 
			       install ours.  So retry process */

			    continue;
			} else
			  check_amoeba_error(status,
					     "msg_grp_join, sp_append");
		    }
		} else {
		    /* entry does exist, but it was empty, so use
		       sp_install. */

		    oldcap = NULL;
		    dir_sp_entry.se_name = gname;
		    status = sp_install(1, &dir_sp_entry, &gcapset, &oldcap);
		    if (status) {
			if (status == SP_CLASH) {
			    /* Another process has successfully installed a 
			       port in the interval after we determined that no
			       capability was present and when we tried to 
			       install ours.  So retry process */

			    continue;
			} else
			  check_amoeba_error(status,
					     "msg_grp_join, sp_install");
		    }
		}

		joining_mode = GRP_CREATE;
		gp->grp_handle = ERR_CONVERT(
				  grp_create(&grp_cap.cap_port,
					     (options & MSG_GRP_RESILIENT) ?
					       MAX_GROUP_SIZE : 0,
					     MAX_GROUP_SIZE,
					     6, 12
					     )
				 );

		if (gp->grp_handle < 0)
		  check_amoeba_error(gp->grp_handle,
				     "msg_grp_join, grp_create");

		oldcap = &grp_cap;
		dir_sp_entry.se_name = gname;

		status = sp_install(1, &dir_sp_entry, &gcapset, &oldcap);

		/* One instance of the group exists with this port name AND
		   if any others are existent they must be before their
		   second sp_install, which will fail, resulting in that group
		   being deleted */

		if (status) {
		    if (status == SP_CLASH) {
			continue;
		    } else
		      check_amoeba_error(status, "msg_grp_join, sp_install");
		}
	    } else
	      check_amoeba_error(status, "msg_grp_join, sp_lookup");
	} else {
	    if (cs_to_cap(&gcapset, &grp_cap) != STD_OK)
	      msg_fatal_error("msg_grp_join, cs_to_cap");

	    hdr.h_port = grp_cap.cap_port;
	    hdr.h_command = GRP_JOIN_REQ;
	    hdr.h_extra = 0;
	    hdr.h_size = 0;

	    if (gp->grp_num_domains) {
		uniqport(&gp->grp_xfer_gaddr);
		priv2pub(&gp->grp_xfer_gaddr, &gp->grp_xfer_paddr);
		hdr.h_signature = gp->grp_xfer_paddr;
	    }

	    joining_mode = GRP_JOIN_REQ;

	    gp->grp_handle = grp_join(&hdr);

	    if ((gp->grp_handle == BC_NOEXIST) ||
		(gp->grp_handle == BC_FAIL)) {

		/* delete old capability, but only if it is the one we read
		   (might not be if another process is concurrently trying to
		   join group).  So handle by doing an install of an empty
		   capset */

		capset empty_capset;

		empty_capset.cs_suite = NULL;
		cs_free(&empty_capset);
		dir_sp_entry.se_name = gname;
		oldcap = &grp_cap;

		status = sp_install(1, &dir_sp_entry, &empty_capset, &oldcap);

		if (status == SP_CLASH)
		  continue;	/* delete failed, but that is okay */
		check_amoeba_error(status, "msg_grp_join, sp_install empty");
		continue;
	    } else
	      check_amoeba_error(gp->grp_handle, "msg_grp_join, grp_join");

/*	    printf("successfully joined group...\n");  ***/

	    /* group already exists and we have successfully joined it */

	}

	/* One instance of the group exists with this port name AND
	   if any others are existent they must be before their
	   second sp_install, which will fail, resulting in that group
	   being deleted */

	break;
    }

    gp->grp_port = grp_cap.cap_port;

#ifdef DEBUG
    if (msg_trace)
      printf("joined group %s\n", ar_port(&gp->grp_port));
#endif

    /* call the group membership change routine */
    group_member_change(gp, joining_mode, NULL);

    if (!(options & MSG_GRP_MANUAL_RCV))
      _t_fork(call_msg_grp_listener, (void *) gp, "msg_grp_listener");

    return gp;
}


static errstat my_grp_reset(gp)
GROUP *gp;

{
    header hdr;
    int err;

    hdr.h_port = gp->grp_port;
    hdr.h_command = GRP_RESET;
    hdr.h_extra = 0;
    hdr.h_size = 0;

    err = ERR_CONVERT(grp_reset(gp->grp_handle, &hdr, 1));

    while (err < 0) {
	if (err != BC_ABORT)
	  check_amoeba_error(err, "my_grp_reset, grp_reset");

	hdr.h_port = gp->grp_port;
	hdr.h_command = GRP_RESET;
	hdr.h_extra = 0;
	hdr.h_size = 0;

#ifdef DEBUG
	if (msg_trace)
	  printf("calling group reset\n");
#endif

	err = ERR_CONVERT(grp_reset(gp->grp_handle, &hdr, 1));

    }

    return err;

    /* note that we do not call group_member_change here; that will happen
       when the reset message is received */

}



void msg_grp_leave(gp)
GROUP *gp;

/* Interface routine to leave the group.

   Could (should ?) also delete capability */

{
    header hdr;
    int status;

    hdr.h_port = gp->grp_port;
    hdr.h_command = GRP_LEAVE;
    hdr.h_extra = 0;
    hdr.h_size = 0;

    status = grp_leave(gp->grp_handle, &hdr);
    check_amoeba_error(status, "msg_grp_leave, grp_leave");
}



int msg_grp_send(gp, entry, msg)
     GROUP *gp;
     int entry;
     message *msg;

{
    int status;
    header hdr;

    if (!msg || !gp || (entry < 0))
      msg_fatal_error("bad parm to msg_grp_send");

    hdr = msg->hdr;
    hdr.h_extra = GRP_MY_ID(gp) | MSG_OPTIONS(msg);
    hdr.h_port = gp->grp_port;
    hdr.h_command = entry;
    hdr.h_size = MSG_SIZE(msg);

    status = grp_send(gp->grp_handle, &hdr, msg->data, MSG_SIZE(msg));

    while (ERR_STATUS(status)) {
	if (status == BC_ABORT) {
	    status = my_grp_reset(gp);
	    check_amoeba_error(status, "msg_grp_send, my_grp_reset");
	} else if (status != BC_FAIL)
	  check_amoeba_error(status, "msg_grp_send, grp_send");
	/* For BC_FAIL, we just try again */

	hdr.h_extra = gp->grp_state.st_myid | MSG_OPTIONS(msg);
	hdr.h_port = gp->grp_port;
	hdr.h_command = entry;
	hdr.h_size = MSG_SIZE(msg);
	status = grp_send(gp->grp_handle, &hdr, msg->data, MSG_SIZE(msg));
    }

    return 0;
}



message *msg_grp_receive(gp)
GROUP *gp;

/* returns next message from GROUP gp.  Handles resets transparently */

{
    register message *mp;
    register int size;
    int more;
    char *sender_name;

    mp = _alloc_msg();

    /* Loop until we get a non-system message */
    for (;;) {
	mp->hdr.h_port = gp->grp_port;

	size = ERR_CONVERT(
			   grp_receive(gp->grp_handle, &mp->hdr,
				       mp->data, MSG_MAXSIZE, &more)
			   );

	while (size < 0) {
	    if (check_amoeba_error(size, "msg_grp_receive, grp_receive") ==
		BC_ABORT)
	      my_grp_reset(gp);

	    mp->hdr.h_port = gp->grp_port;
	    size = ERR_CONVERT(
			       grp_receive(gp->grp_handle, &mp->hdr, mp->data,
					   MSG_MAXSIZE, &more)
			       );
	}

	if (size != mp->hdr.h_size)
	  msg_fatal_error("msg_grp_receive, size rcvd != size expected");

#ifdef DEBUG
	if (msg_trace & TRACE_MSGS) {
	    int entry = MSG_ENTRY(mp);
	    char *pmsg = "msg_grp_receive, rcvd msg";

	    if (entry == STD_DESTROY)
	      printf("%s STD_DESTROY\n", pmsg);
	    else if (entry == GRP_LEAVE)
	      printf("%s GRP_LEAVE\n",pmsg);
	    else if (entry == GRP_RESET)
	      printf("%s GRP_RESET\n", pmsg);
	    else if (entry == GRP_JOIN_REQ)
	      printf("%s GRP_JOIN_REQ\n", pmsg);
	    else if (MSG_OPTIONS(mp) & MSG_ERR_FORWARD)
	      printf("%s forwarded rpc err %d [%d]\n",
		     pmsg, MSG_GRP_SENDER_ID(mp), MSG_SEQNUM(mp));
	    else if (MSG_OPTIONS(mp) & MSG_REQ_FORWARD)
	      printf("%s forwarded rpc req %d [%d]\n",
		     pmsg, MSG_GRP_SENDER_ID(mp), MSG_SEQNUM(mp));
	    else if (MSG_OPTIONS(mp) & MSG_RPLY_FORWARD)
	      printf("%s forwarded rpc reply %d [%d]\n",
		     pmsg,MSG_GRP_SENDER_ID(mp), MSG_SEQNUM(mp));
	    else
	      printf("%s grp entry %s (%d)\n",pmsg,
		     msg_grp_entry_name(gp, entry));
	}
#endif	    

	if (MSG_ENTRY(mp) >= GRP_FIRST_COMMAND) {
	    if (MSG_ENTRY(mp) == STD_DESTROY) {
		extern char *program_name;

		printf("Process %s received STD_DESTROY!\n", program_name);
		exitprocess(0);
	    } else {
		group_member_change(gp, MSG_ENTRY(mp), mp);
		continue;
	    }
	} else
	  break;		/* we've got a valid message to return */
    }

    mp->end_ptr = mp->data + mp->hdr.h_size;
    mp->start_ptr =
      mp->next_ptr = buf_get_string(mp->data + MSG_OFFSET_CTXTNAME, 
				    MSG_END(mp),&sender_name);

    if (!mp->start_ptr)
      msg_fatal_error("msg_grp_rcv, buf_get");
    mp->ctxtp = ctx_lookup_context(sender_name);

    return mp;
}
