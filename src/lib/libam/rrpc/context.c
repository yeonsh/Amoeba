/*	@(#)context.c	1.3	96/02/27 11:04:19 */
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
 * versions reserved by the authors.
 * 
 * context handling routines
 * 
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <ampolicy.h>
#include <rrpc/msg_rpc.h>
#include <capset.h>
#include <module/ar.h>
#include <module/stdcmd.h>
#include <module/prv.h>

#include <server/soap/soap.h>
#include "rrpc_protos.h"


CTXT_NODE *_base_context = NULL;	/* the base context */


#define IS_KERNEL_CTXT(cp) (strncmp((cp)->name, "kernel(", 7) == 0)
#define KERNEL_CAP(cp) ((cp)->name + 7)


#define CTXT_TABLESIZE 37
#define HASH_CONTEXT(n) (hashpjw(n) % CTXT_TABLESIZE)


#define CHUNK	15		/* memory allocation unit.  It is too
				   inefficient to call the system
				   allocator for space for every
				   identifier. Instead, we call for large
				   chunks, and divide them up as necessary 
				   */

/* NB:  kernel version does not run context_alive and monitor checking.
   This means, for example that the kernel should not itself subscribe to
   another context with the hope of being notified when that context comes
   back up (we may want to enable that at some point).
 */


void sleep_ms(milliseconds)
int milliseconds;

/* a somewhat crude way of "sleeping" for a specified number of milliseconds
   in Amoeba... */

{
    semaphore semi;

    sema_init(&semi, 0);
    sema_trydown(&semi, milliseconds);
}



static CTXT_NODE *ctxt_hashtable[CTXT_TABLESIZE];
CTXT_NODE *context_list = NULL;

int  hashpjw(name)
/* hash function basically as in section 7.6 of Dragon Book. */

char *name;

{
    register unsigned   h = 0,
                        g;

    for (; *name; name++) {
        h = (h << 4) + (int) *name;
        if (g = h & 0xf000) {
            h = h ^ (g >> 12);
            h ^= g;
        }
    }
    return h;
}



static void remove_context(cp)
CTXT_NODE *cp;

/* removes context from linked lists and frees associated data */

{
    CTXT_NODE *hp;
    int hashvalue;

    printf("removing context %s\n", cp->name);

    /* First remove from the hash table */

    hashvalue = HASH_CONTEXT(cp->name);
    hp = ctxt_hashtable[hashvalue];
    if (cp->prev)
      cp->prev->next = cp->next;
    if (cp->next)
      cp->next->prev = cp->prev;
    if (hp == cp)
      ctxt_hashtable[hashvalue] = NULL;

    /* Now remove from the global list of linked contexts */

    if (cp->prev_context)
      cp->prev_context->next_context = cp->next_context;
    if (cp->next_context)
      cp->next_context->prev_context = cp->prev_context;
    if (context_list == cp)
      context_list = NULL;

    /* delete message associated with context.  We set the context to zero
       so that msg_delete won't try to call delete_context_ref on it.  Not
       that it should cause a problem if it did, but it is ugly */
       
    if (cp->last_reply) {
	cp->last_reply->ctxtp = NULL;
	msg_delete(cp->last_reply);
    }

    free(cp);
}



void _delete_context_ref(cp)
CTXT_NODE *cp;

/* decrements the refcnt for the given context */

{
    if (!cp->local)
      cp->refcnt--;
}


/*ARGSUSED*/
static void context_reaper(arg)
void *arg;

/* Thread garbage-collects contexts which are no longer active */

{
    CTXT_NODE *cp, *next;
    capability cap;

    while (1) {

	sleep_ms(300000);

	for (cp = context_list; cp; cp = next) {
	    next = cp->next_context;
	    if ((!cp->local) && (cp->refcnt == 0)) {
		cap.cap_port = cp->addr;

		printf("context_reaper, trying %s\n", cp->name);

		if ((std_touch(&cap) != STD_OK) &&
		    addr_isequal(&cap.cap_port, &cp->addr)) {
		    /* it could be that while we are doing the touch, the
		       address could be updated.  So we must carry out
		       the delete only if the current addr is the addr
		       which was found to be dead. */

		    extern GROUP *_replicated_grp;
		    extern int _replicated_mode;

		    if (_replicated_mode) {
			message *msg = msg_newmsg();
			msg_put_string(msg, cp->name);
			paddr(&cp->addr);printf(" being reap-tested\n");
			msg_put_address(msg, &cp->addr);
			MSG_OPTIONS(msg) = MSG_CTXT_RMV;
			msg_grp_send(_replicated_grp, 0, msg);
			msg_delete(msg);
		    } else {
			/* should have a mutex here if using preemptible
			   threads */
			if (!cp->refcnt) {
			    /* need to make sure that there are still no
			       references to this context before deleting it */
			    remove_context(cp);
			}
		    }
		}
	    }
	}
    }
}
		


void _replicated_context_remove(msg)
message *msg;

{
    address addr;
    CTXT_NODE *cp;
    char *name;

    msg_get_string(msg, &name);
    if (msg_get_address(msg, &addr))
      msg_fatal_error("replica_context_remove, msg_get");
    cp = ctx_lookup_context(name);
    _delete_context_ref(cp);	/* context ref for this msg doesn't count */
    msg->ctxtp = NULL;

    if (cp->refcnt)
      printf("replicated_context_remove, nonzero refcnt\n");

    if (addr_isequal(&addr, &cp->addr) && (!cp->refcnt))
      remove_context(cp);
}



CTXT_NODE * ctx_lookup_context(context_name)
char *context_name;

/* Inserts context name into context table */

{
    int     hashvalue;          /* hash value */
    int     compvalue = 1;      /* result of comparison */
    CTXT_NODE *nextnode;
    register CTXT_NODE *previous = NULL;
    register CTXT_NODE *current;

    current = ctxt_hashtable[hashvalue = HASH_CONTEXT(context_name)];

    while ((current != NULL) &&
            ((compvalue = strcmp(current->name, context_name)) < 0)) {
	/* entries are kept in sorted order, so we don't need
	   to go through entire list */
        previous = current;
        current = current -> next;
    }

    if (!compvalue) {
	current->refcnt++;
	return current;		/* context already exists */
    } else {
	nextnode = (CTXT_NODE *) malloc(sizeof(CTXT_NODE));
	memset(nextnode, 0, sizeof(CTXT_NODE));

	if (previous != NULL) {
	    previous -> next = nextnode;
	    nextnode -> prev = previous;
	} else
	    ctxt_hashtable[hashvalue] = nextnode;

	nextnode->next = current;
	nextnode->name = _str_malloc(context_name);
        nextnode->is_null = TRUE;
	nextnode->next_context = context_list;
	nextnode->prev_context = NULL;
	nextnode->refcnt = 1;
	context_list = nextnode;
        return nextnode;
    }
}


/* routines for looking up/installing context addresses */


static capability ctxt_dir_cap;	/* directory where addresses are kept */
static capset ctxt_dir_capset;	/* for SOAP */
static int init_ctxt_dir = TRUE; /* do the above need initializing? */


static void init_ctxt()

{
    int status;

    status = name_lookup(RRPC_CTX_DIR, &ctxt_dir_cap);
    check_amoeba_error(status, "init_ctxt, name_lookup");
    init_ctxt_dir = FALSE;
    /* We need a capset structure to call sp_install */
    if (!cs_singleton(&ctxt_dir_capset, &ctxt_dir_cap))
      msg_fatal_error("init_ctxt, cs_singleton");
}


static address *lookup_context_address(ctxtp)
CTXT_NODE *ctxtp;

/* Gets from directory service the address for the given context.  The
   context is "pinged" using std_touch to see if it is alive.  If the
   context can be contacted successfully, then its current address is
   returned, otherwise &NULLADDRESS is returned */

{
    static capability cap;
    capset ctxt_capset;
    int status;

#ifdef KERNEL
    printf("Kernel trying to lookup address of %s\n", ctxtp->name);
    return &NULLADDRESS;
#endif

    if (IS_KERNEL_CTXT(ctxtp)) {
	capset kcapset;
	capability kcap;

	ar_tocap(KERNEL_CAP(ctxtp), &kcap);

	printf("looking up kernel(%s)\n", ar_cap(&kcap));

	cs_singleton(&kcapset, &kcap);
	status = sp_lookup(&kcapset, "Meta", &ctxt_capset);
	cs_free(&kcapset);
    } else {
	if (init_ctxt_dir)
	  init_ctxt();

	/* get address from directory server */
	status = sp_lookup(&ctxt_dir_capset, ctxtp->name, &ctxt_capset);
    }

    if (status == STD_OK) {
	if (cs_to_cap(&ctxt_capset, &cap) != STD_OK)
	  msg_fatal_error("lookup_ctxt_addr, cs_to_cap");
	cs_free(&ctxt_capset);
	/* make sure address is still valid */
	if (std_touch(&cap) == STD_OK)
	  return &cap.cap_port;
	else
	  return &NULLADDRESS;
    } else if ((status == STD_NOTFOUND) || (status == SP_UNAVAIL)) {
	return &NULLADDRESS;
    } else {
	check_amoeba_error(status,"lookup_ctxt_addr, sp_lookup");
	return &NULLADDRESS;
    }
}



static address *context_address(ctxtp)
CTXT_NODE *ctxtp;

/* Should it be protected with mutex?  The only competing thread would be
   monitor_context.  In non-preemptive thread model, this protection is
   unnecessary.  So currently omitted.

   The is_null field is used to ensure that changes to the address are
   seen as virtually synchronous events.  For convenience sake, routines that
   see a new address directly update the address before calling the
   context_alive routine.  However, the new address must not be used until
   the context_alive routine has run.
*/

{
    if (!ctxtp->addr_lookedup || ctxtp->is_null) {
	ctxtp->addr = *lookup_context_address(ctxtp);
	ctxtp->addr_lookedup = TRUE;
	ctxtp->is_null = addr_isnull(&ctxtp->addr);
    }

    if (ctxtp->is_null)
      return &NULLADDRESS;
    else
      return &ctxtp->addr;
}



static void install_ctxt_addr(ctxtp, addr)
     CTXT_NODE *ctxtp;
     address *addr;

/* installs the "put" address for this context, using the specified address.
   If addr is a NULL pointer, then it generates one. */

{
    int status;
    capability cap;
    capset ctxt_capset;
    static long mask[3]  = {0xff, 0, 0};

#ifndef KERNEL
    if (init_ctxt_dir)
      init_ctxt();
#endif

    if (addr)
      ctxtp->get_port = *addr;
    else
      uniqport(&ctxtp->get_port);

    ctxtp->is_null = FALSE;
    priv2pub(&ctxtp->get_port, &ctxtp->addr);
    cap.cap_port = ctxtp->addr;
    uniqport(&ctxtp->check_field);
    if (prv_encode(&cap.cap_priv, (objnum) 0,
		   PRV_ALL_RIGHTS, &ctxtp->check_field))
      msg_fatal_error("install_ctxt_addr, prv_encode");

    if (!cs_singleton(&ctxt_capset, &cap))
      msg_fatal_error("install_ctxt_addr, cs_singleton");

#ifdef KERNEL
    if (1) {
	capset kcapset;
	capability kcap;

	status = name_lookup("/", &kcap);
	check_amoeba_error(status, "install_ctxt_addr, name_lookup");
	/* We need a capset structure to call sp_install */
	if (!cs_singleton(&kcapset, &kcap))
	  msg_fatal_error("install_ctxt_addr, cs_singleton");
	
	status = sp_append(&kcapset, "Meta", &ctxt_capset, 3, mask);
	if (status == STD_EXISTS) {
	    status = sp_replace(&kcapset, "Meta", &ctxt_capset);
	}
	cs_free(&kcapset);
    } 

#else

    status = sp_append(&ctxt_dir_capset, ctxtp->name, &ctxt_capset,
		       3, mask);
    if (status == STD_EXISTS) {
	status = sp_replace(&ctxt_dir_capset, ctxtp->name, &ctxt_capset);
    }

#endif

    cs_free(&ctxt_capset);
    check_amoeba_error(status, "install_ctxt_addr, sp_append/replace");
}



/* Stuff for handling replicated RPC calls */


int _my_rep_grpid;		/* if replicated, group id */
int _replicated_mode = 0;	/* am I replicated? */
GROUP *_replicated_grp = NULL;	/* group for replication */
message *_queued_reply_list = NULL; /* list of queued RPC replies */
SIGNAL _reply_or_mchange;	/* used to tell cohorts a reply or a */
				/* membership change has occurred */

extern int _multiple_replicas;
extern int _coordinator;

/*ARGSUSED*/
static void replica_membership_change(gp, arg)
GROUP *gp;
void *arg;

{
    static int current_coordinator;

    printf("rrpc: replica membership changed\n");

/*    printf("coordinator %d, my id %d, id[0] %d id[1] %d %d\n",
	   _coordinator, GRP_MY_ID(gp), gp->grp_mem_id[0], gp->grp_mem_id[1],
	   GRP_NUM_MEMBERS(gp)); *****/

    if (_coordinator) {
	if (!current_coordinator) {
	    /* once coordinator, always coordinator ... */
	    current_coordinator = TRUE;
	    _t_fork(context_reaper, (void *) NULL, "context_reaper");
	}
    }

    _sig_signal(&_reply_or_mchange);
}



static void set_replicated_group(gp)
GROUP *gp;

{
    if (_replicated_mode)
      msg_fatal_error("only one replicated context per server!");

    _replicated_mode = TRUE;
    _replicated_grp = gp;
    _my_rep_grpid = GRP_MY_ID(gp);
}



static message * ctxt_state_xfer(seq) 
int seq;

/* transfer the context's state.  We transfer information for all
   contexts that we have a last_reply message for. */

{
    static CTXT_NODE *cp;
    message *msg;

    if (seq == 0) {
	int num_contexts;

	/* before doing anything, make sure all replies have been generated.
	   Note that since no new messages will be delivered during the
	   state transfer, any backlog will eventually be cleared (ie., we
	   won't wait here forever) */

	for (cp=context_list; cp; cp=cp->next) {
	    if (cp->last_replied_seqnum < cp->last_seqnum) {
#ifdef DEBUG
		printf("ctxt_state_xfer awaiting reply");
#endif
		_sig_wait(&cp->new_reply);
	    }
	}

	/* count the number of contexts for which we have a reply */
	for (num_contexts = 0, cp = context_list; cp; cp=cp->next)
	  if (cp->last_reply)
	    num_contexts++;

	/* create base message containing 
	   number of contexts,  _base_context->last_seqnum,
	     followed by, for each context,
	   (name, address, last_seqnum) */

	msg = msg_newmsg();
	msg_put_int(msg, num_contexts);
	msg_put_int(msg, _base_context->last_seqnum);

#ifdef DEBUG
	if (msg_trace & TRACE_REP)
	  printf("xfering state, _base_context %d, num_contexts %d\n",
		 _base_context->last_seqnum, num_contexts);
#endif

	for (cp=context_list; cp; cp=cp->next) {
	    if (cp->last_reply) {
		msg_put_string(msg, cp->name);
		msg_put_address(msg, &cp->addr);
		msg_put_int(msg, cp->last_seqnum);

#ifdef DEBUG
		if (msg_trace & TRACE_REP)
		  printf("context %s, %d\n", cp->name, cp->last_seqnum);
#endif

	    }
	}
	cp = context_list;
	return msg;
    } else {
	while (cp && !cp->last_reply)
	  cp = cp->next;

	if (cp) {
	    msg = cp->last_reply;
	    msg_increfcount(msg);
	    cp = cp->next;
	    return msg;
	} else
	  return NULL;
    }
}



static int ctxt_state_rcv(seq, mp)
     int seq;
     message *mp;

{
    static message *base_msg;
    static int num_contexts;
    char *name;
    CTXT_NODE *cp;

    if (seq == 0) {
	base_msg = mp;
	msg_increfcount(mp);
	msg_get_int(mp, &num_contexts);
	msg_get_int(mp, (int *) &_base_context->last_seqnum);

#ifdef DEBUG
	if (msg_trace & TRACE_REP)
	  printf("_base_context seqnum set to %d, num_contexts %d\n",
		 _base_context->last_seqnum, num_contexts);
#endif

    } else {
	if (!num_contexts--)
	  msg_fatal_error("ctxt_state_rcv, unexpected reply");
	msg_get_string(base_msg, &name);
	cp = ctx_lookup_context(name);
	msg_get_address(base_msg, &cp->addr);
	msg_get_int(base_msg, (int *) &cp->last_seqnum);
	cp->last_replied_seqnum = cp->last_seqnum;
	cp->last_reply = mp;
	_delete_context_ref(cp);	/* this ref shouldn't count */
	msg_increfcount(mp);

	if (!num_contexts) {
	    msg_delete(base_msg);
	}
    }
    return 0;
}

static void
call_msg_listener(arg)
void *arg;
{
	msg_listener((CTXT_NODE *) arg);
}


CTXT_NODE *ctx_define_context(context_name,is_replicated,num_state_domains,
			      state_xfer_routines,state_rcv_routines)

char *context_name;
int is_replicated;
int num_state_domains;
message *(*state_xfer_routines[]) _ARGS((int));
int (*state_rcv_routines[]) _ARGS((int, message *));
		       
/* Creates context record and updates appropriately.  Creates a port and
   registers it with the name server.  Sets *ctxtp_p to point to the
   created context (done this way for historical reasons (ISIS version)).
 */

{
    CTXT_NODE *ctxtp;
    extern message *_empty_msg;

    ctxtp = ctx_lookup_context(context_name);

    if (ctxtp->local)
      return ctxtp;		/* this is a repeat action, so ignore */

    ctxtp->local = TRUE;

    _base_context = ctxtp;

    if (!_empty_msg) {
	_empty_msg = msg_newmsg();
    }

    if (is_replicated) {
	int i;
	message *(*xfer_funcs[MAX_DOMS]) _ARGS((int));
	int (*rcv_funcs[MAX_DOMS]) _ARGS((int, message *));

	GROUP *gp;

	xfer_funcs[0] = ctxt_state_xfer;
	rcv_funcs[0] = ctxt_state_rcv;

	if (num_state_domains > MAX_DOMS)
	  msg_fatal_error("ctx_define_context, too many state domains");

	for (i=num_state_domains; i; i--) {
	    xfer_funcs[i] =
	      state_xfer_routines[i - 1];
	    rcv_funcs[i] =
	      state_rcv_routines[i - 1];
	}

	_sig_init(&_reply_or_mchange);
	gp = msg_grp_join(ctxtp->name, MSG_GRP_RESILIENT,
			   num_state_domains + 1, xfer_funcs, rcv_funcs,
			   replica_membership_change, NULL);

	set_replicated_group(gp);

	install_ctxt_addr(ctxtp, &gp->grp_port);

#ifdef DEBUG	
	if (msg_trace & TRACE_REP) {
	    printf("replicated context ");
	    printf("on line.\n");
	}
#endif

    } else {
	install_ctxt_addr(ctxtp, NULL);
    }

    /* Fork off some threads to handle messages */


#ifdef PERF_SEQ
    if (!is_replicated || _coordinator) {   /**PERF HACK --- sequencer***/
#endif

#ifdef PERF_NSEQ
    if (!_multiple_replicas || !_coordinator) { /* PERF HACK --- sequencer doesn't handle */
#endif

	_t_fork(call_msg_listener, (void *) ctxtp, "msg_listener1");
	_t_fork(call_msg_listener, (void *) ctxtp, "msg_listener2");

#ifdef PERF
    }				/* PERF HACK */
#endif

    return ctxtp;
}



#define CTXT_POLL_PERIOD 10000

static void context_failed(arg)
void *arg;

/* context has failed.  This is separate from monitor_context since the
   msg_ctxt_rpc and check_seqnum routines may also call it. */

{
    CTXT_NODE *ctxtp = (CTXT_NODE *) arg;

    ctxtp->is_null = TRUE;
    ctxtp->addr = NULLADDRESS;

#ifdef DEBUG
    if (msg_trace & TRACE_MSGS)
      printf("context %s failed\n", ctxtp->name);
#endif

    /* Hooks into the various Meta routines that care about a context failed.
       Probably should modify watch_context routine so that a handle routine
       is passed; that routine is called with the context.  But we want the
       address change to be seen virtually synchronously by Meta and to get
       that effect without imposing an extra layer, we need to run the address
       update as a MSG_MUTEX event. */

}


static void context_alive(ctxtp)
CTXT_NODE *ctxtp;

/* context alive.  This is separate from monitor_context since the
   check_seqnum routine may also call it. */

{

    ctxtp->is_null = FALSE;

#ifdef DEBUG
    if (msg_trace & TRACE_MSGS)
      printf("context %s alive\n", ctxtp->name);
#endif

}



static void force_context_addr_change(msg)
message *msg;

/* called by check_seqnum when it has received a message from a context with
   that context's address being different from what is locally cached.  This
   causes a virtually synchronous change of context address */

{
    CTXT_NODE *cp;
    int seqnum;
    char *ctxt_name;
    address *ctxt_addr;

    msg_get_stamp(msg, &ctxt_addr, &seqnum, &ctxt_name);
    cp = msg->ctxtp;

/*    printf("RUNNING force_context_change, context %s\n", cp->name); */

    if (!cp->is_null)
      context_failed(cp);
    cp->addr = *ctxt_addr;

/*    printf("SEQNUM force_context_addr_change, seqnum = %d\n"); */
    cp->last_seqnum = seqnum;
    context_alive(cp);

}



int ctx_rpc(ctxtp, entry, req_mp, reply)
     CTXT_NODE *ctxtp;
     int entry;
     message *req_mp;
     message **reply;

/* Makes an RPC call to the context pointed to by ctxtp, sending message
   req_mp destined for entry req_mp.  *reply is set to point to the reply
   message.
   */

{
    int status = 0;
    address addr;

    /* we get the address through context_address as that will actually
       fetch the address for us if it can.  Address changes must be seen
       as "virtually synchronous" events and therefore can only be changed
       by the context_alive/failed routines---except that it is also safe
       the first time we reference an address to also go out and fetch it */


    /* tag the message with its sequence number */
    MSG_SEQNUM(req_mp) = ++_base_context->last_seqnum;

    if (_multiple_replicas) {

	/* note that we currently do not run agreement on the result of the
	   context_address call.  This means that context changes are not
	   guaranteed to be seen by all replicas in the same order.  However,
	   we do guarantee that as long as calls are via msg_ctxt_rpc, then all
	   replicas will see the same sequence of replies and RPC_NOTFOUND's */

	/* we handle the case where a reply is expected and the case where it
	   is not expected separately */

	if (reply) {
	    *reply = msg_head_of_queue(&_queued_reply_list);
	    while (!*reply) {
		if (_coordinator) {
		    addr = *context_address(ctxtp);
		    if (!addr_isnull(&addr)) { 
			do {
			    status = msg_rpc(&addr, entry, req_mp, reply);
			} while (status == RPC_FAILURE);
		    } else
		      status = RPC_NOTFOUND;

		    /* need to send on error messages.  As coded, an error
		       other than RPC_FOUND will cause the coord to fail in
		       check_amoeba_error while others will see it. 
		       But---these are just safety checks... */

		    if ((status == RPC_NOTFOUND)) {
			message *err_msg = msg_newmsg();

			MSG_SEQNUM(err_msg) = _base_context->last_seqnum;
			MSG_OPTIONS(err_msg) =
			  MSG_RPLY_FORWARD | MSG_ERR_FORWARD;
			msg_put_int(err_msg, status);

			/* the entry point doesn't matter in this case. */
			msg_grp_send(_replicated_grp, 0, err_msg);
			msg_delete(err_msg);
			return status;
		    }

		    check_amoeba_error(status, "msg_ctxt_rpc, msg_rpc");

		    MSG_OPTIONS(*reply) = MSG_RPLY_FORWARD;
		    /* entry point doesn't matter in this case. */

		    msg_grp_send(_replicated_grp, 0, *reply);
		    return status;	/* status better be zero here! */
		} else {
		    extern SIGNAL _reply_or_mchange;
		    _sig_wait(&_reply_or_mchange);
		    *reply = msg_head_of_queue(&_queued_reply_list);
		}
	    }

	    if (MSG_SEQNUM(*reply) != _base_context->last_seqnum) {
#ifdef DEBUG
		printf("rcvd seqnum %d, expected %d\n",
		       MSG_SEQNUM(*reply), _base_context->last_seqnum);
#endif
		msg_fatal_error("msg_ctxt_rpc, mismatched reply");
	    }

	    if (MSG_OPTIONS(*reply) & MSG_ERR_FORWARD) {
		if (msg_get_int(*reply, &status))
		  msg_fatal_error("msg_get_int, MSG_ERR_FORWARD");
		msg_delete(*reply);
	    }

	} else {
	    /* Like if (reply) case but optimized for no reply */
	    message *dummy_reply;

	    dummy_reply = msg_head_of_queue(&_queued_reply_list);

	    while (!dummy_reply) {
		if (_coordinator) {
		    message *result_msg = msg_newmsg();

		    addr = *context_address(ctxtp);
		    if (!addr_isnull(&addr)) { 
			do {
			    status = msg_rpc(&addr, entry, req_mp, NULL);
			} while (status == RPC_FAILURE);
		    } else
		      status = RPC_NOTFOUND;
		    
		    MSG_SEQNUM(result_msg) = _base_context->last_seqnum;
		    MSG_OPTIONS(result_msg) =
		      MSG_RPLY_FORWARD | MSG_ERR_FORWARD;

		    msg_put_int(result_msg, status);
		    msg_grp_send(_replicated_grp, 0, result_msg);
		    msg_delete(result_msg);
		    return status;
		} else {
		    extern SIGNAL _reply_or_mchange;
		    _sig_wait(&_reply_or_mchange);
		    dummy_reply = msg_head_of_queue(&_queued_reply_list);
		}
	    }

	    if (MSG_SEQNUM(dummy_reply) != _base_context->last_seqnum)
	      msg_fatal_error("msg_ctxt_rpc, mismatched reply");

	    if (MSG_OPTIONS(dummy_reply) & MSG_ERR_FORWARD) {
		if (msg_get_int(dummy_reply, &status))
		  msg_fatal_error("msg_get_int, MSG_ERR_FORWARD");
		msg_delete(dummy_reply);
	    } else
	      msg_fatal_error("msg_ctxt_rpc, unexpected reply");

	}

    } else {

	if (msg_queued_msg_avail(_queued_reply_list)) {
	    message *qd_reply;

	    qd_reply = msg_head_of_queue(&_queued_reply_list);

	    if (MSG_SEQNUM(qd_reply) != _base_context->last_seqnum)
	      msg_fatal_error("msg_ctxt_rpc, mismatched qd_reply");

	    if (MSG_OPTIONS(qd_reply) & MSG_ERR_FORWARD) {
		if (msg_get_int(qd_reply, &status))
		  msg_fatal_error("msg_get_int, MSG_ERR_FORWARD");
		msg_delete(qd_reply);
	    } else {
		if (*reply)
		  *reply = qd_reply;
		else
		  msg_delete(qd_reply);
	    }
	} else {
	    addr = *context_address(ctxtp);
	    if (!addr_isnull(&addr)) {
		do {
		    status = msg_rpc(&addr, entry, req_mp, reply);
		} while (status == RPC_FAILURE);

		if (status == RPC_NOTFOUND) {
		    _t_fork(context_failed, (void *) ctxtp, "context_failed");
		}
	    } else
	      status = -1;
	}

    }

    return status;
}



int _check_seqnum(msg)
message *msg;

/* returns TRUE if message should be dropped, ie., if it is redundant.
   
 */

{
    CTXT_NODE *cp;
    int seqnum;
    char *ctxt_name;
    address *ctxt_addr;

    msg_get_stamp(msg, &ctxt_addr, &seqnum, &ctxt_name);
    cp = msg->ctxtp;

#ifdef DEBUG
    if (msg_trace & TRACE_MSGS) {
	printf("CHK_SEQNUM %s seq # %d (%d)\n",
	       ctxt_name, seqnum, cp->last_seqnum);
    }
#endif

    if (cp->local)
      return FALSE;		/* ones from ourself are always new! */
    else if (addr_isequal(ctxt_addr, &cp->addr)) {
	if (seqnum <= cp->last_seqnum)
	  return TRUE;
	else {
	    cp->last_seqnum = seqnum;
	    return FALSE;
	}
    } else {
	/* we've never seen this address before.  So we need to consider
	   old one to have failed, and make this the new address */

	if (cp->is_null && !cp->addr_lookedup) {
	    /* don't need to call context_alive if we've never looked up
	       the address before, since that implies we weren't watching it */
	    cp->is_null = FALSE;
	    cp->addr_lookedup = TRUE;
	    cp->addr = *ctxt_addr;
	    cp->last_seqnum = seqnum;
	} else {
	    /* This new address must be made into a virtually synchronous
	       event.  Consequently we fork off a separate thread to trigger
	       the new_context (with the separate thread being necessary
	       in case any RPC calls must be performed---Amoeba restriction).
	       We block until this thread is done.  NB!  Slight possible
	       problem w/ the way we are doing it here:  if we hear yet another
	       address from this context while the (previous)
	       force_context_addr_change is running, we can have a race
	       condition */

/*	    printf("%s: %s, is_null %d, lookedup %d, last %d, seq num %d\n",
		   _base_context->name,
		   cp->name, cp->is_null, cp->addr_lookedup,
		   cp->last_seqnum, seqnum); *** */

	    force_context_addr_change(msg);
	}
	return FALSE;
    }
}



struct msg_entry ctx_entry_table[MAX_MSG_ENTRIES];


void ctx_define_entry(entry, func, entry_name)
int entry;
message *(*func) _ARGS((message *));
char *entry_name;

/* Defines this entry number for handling messages.

   Entries are somewhat like the ISIS entries, with the key difference being
   that the message is not deleted after the entry point exits; the entry
   handler must do that itself.  The thread msg_listener continually listens
   for messages to arrive.

   Basic operation:  marks this procedure as being the handler for the messages
   coming in with the specified entry point.  The entry number must be in range
   0 ... (MAX_MSG_ENTRIES - 1).

 */

{
    if (_base_context) {
	msg_fatal_error("ctx_define_entry must be called before ctx_define_context");
    }

    if ((entry < 0) || (entry >= MAX_MSG_ENTRIES))
      msg_fatal_error("msg_entry, bad entry number");

    ctx_entry_table[entry].func = func;
    ctx_entry_table[entry].name = entry_name;
    ctx_entry_table[entry].check_seqnum = TRUE;

#ifdef DEBUG
    if (!func) {
	msg_fatal_error("msg_define_entry, !func");
    }
#endif

}



void msg_listener(ctxtp)
CTXT_NODE *ctxtp;

/* never returns.... This routine listens for messages on the port 
   for the given context, handing incoming messages off to the appropriate
   routine.  The routine is run as a separate thread.

   This routine runs as a separate thread, with one thread being created
   for each context that the stub handles.
 */

{
    message *mp;
    message *reply;
    int entry;
    extern message *_empty_msg;

    while (1) {
	extern int _multiple_replicas;
	extern GROUP * _replicated_grp;
	mp = msg_rpc_rcv(&ctxtp->get_port);

	if (_multiple_replicas) {
	    msg_grp_forward(_replicated_grp, mp);
	    msg_delete(mp);
	    continue;
	}
	entry = MSG_ENTRY(mp);

	if (!ctx_entry_table[entry].func)
	  msg_fatal_error("msg_listener, message for undefined entry point");

	if (ctx_entry_table[entry].check_seqnum && _check_seqnum(mp)) {

	    /* duplicate request; reply with last reply value.

	       We are guaranteed that the reply to the last request is the
	       appropriate reply.  This is true, because we assume that there
	       is at most one pending RPC at a time from a given context; a
	       subsequent RPC is not made until a reply is generated to the
	       previous request.  HOWEVER, that last reply message may have
	       not yet been generated. */

	    if (mp->ctxtp->last_replied_seqnum < MSG_SEQNUM(mp)) {
		if (MSG_SEQNUM(mp) - mp->ctxtp->last_replied_seqnum != 1)
		  msg_fatal_error("MSG_SEQNUM - last_replied_seqnum != 1");
		_sig_wait(&mp->ctxtp->new_reply);
		if (MSG_SEQNUM(mp) != mp->ctxtp->last_replied_seqnum) {
		    printf("mp seqnum %d, last seqnum %d\n",
			   MSG_SEQNUM(mp), mp->ctxtp->last_replied_seqnum);
		    msg_fatal_error("MSG_SEQNUM != last_replied_seqnum");
		}
	    }

	    printf("duplicate, replying with 0x%x:\n", mp->ctxtp->last_reply);
	    msg_printaccess(mp->ctxtp->last_reply);

	    if (mp->ctxtp->last_reply == _empty_msg) {
		/* If this is the empty message that we are replying with,
		   then we need to set the right address.  When we reply
		   with the empty message, we set last_reply to point to
		   the empty message.  Each time this happens, the address
		   part of empty_message gets reset.  Consequently, the address
		   part may no longer be valid---hence we reset it here. */

		mp->ctxtp->last_reply->hdr.h_port = mp->hdr.h_signature;
	    }

	    putrep(&mp->ctxtp->last_reply->hdr, mp->ctxtp->last_reply->data,
		   MSG_SIZE(mp->ctxtp->last_reply));
	    continue;
	}

	/* call appropriate routine to handle this message */

	reply = (*ctx_entry_table[entry].func)(mp);
	msg_rpc_reply(mp, reply);
	if (reply)
	  msg_delete(reply);
	msg_delete(mp);

    }
}
