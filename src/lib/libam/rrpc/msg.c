/*	@(#)msg.c	1.2	96/02/27 11:04:30 */
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
 * Message routines
 */

#define ISSOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amoeba.h"
#include "stdcom.h"
#include "thread.h"
#include "module/ar.h"
#include "module/proc.h"

#include "rrpc/msg_rpc.h"
#include "rrpc_protos.h"


#ifdef DEBUG_ON
int msg_trace = -1;
#else
int msg_trace = 0;
#endif



int msg_numalloc = 0;		/* number of messages currently allocated */
int msg_numfree = 0;		/* number of messages on free list */
int msg_inuse = 0;		/* number of active allocated messages */

address NULLADDRESS;		/* NULL address, initialized to all 0's */


static message *msg_freelist = NULL;	/* the message free list */
message *_empty_msg = NULL;	/* to be used only internally */

static char *type_names[] = {"int", "real", "string", "address"};
#define type_to_name(type) (type_names[type])



char *_str_malloc(value)
char *value;

{
    char *p;

    p = (char *)malloc(strlen(value)+1);
    if (!p)
      msg_fatal_error("str_malloc, malloc");
    return strcpy(p, value);
}


message *msg_head_of_queue(head)
message **head;

/* returns head of queue of messages pointed to by *head, and deletes item
   from queue; queue is maintained as a doubly-linked list to avoid having
   to have separate head & tail pointers. */

{
    message *oldhead =  *head;

    if (oldhead) {
	*head = oldhead->next_msg;
	if (oldhead == *head)
	  *head = NULL;
	else {
	    oldhead->next_msg->prev_msg = oldhead->prev_msg;
	    oldhead->prev_msg->next_msg = oldhead->next_msg;
	}
    }
    return oldhead;
}


void msg_add_to_queue(head, mp)
     message **head;
     message *mp;

/* adds mp to queue of messages pointed to by *head; queue is maintained as
   a doubly-linked list to avoid having to have separate head & tail ptrs. 
*/

{
    if (*head) {
	mp->next_msg = *head;
	mp->prev_msg = (*head)->prev_msg;
	(*head)->prev_msg->next_msg = mp;
	(*head)->prev_msg = mp;
    } else {
	*head =
	  mp->next_msg =
	    mp->prev_msg = mp;
    }
}



/* signal routines.  NB: this implementation assumes nonpremptive scheduling */


void _sig_wait(sp)
     SIGNAL *sp;

/* (should) always block.  Awaits a sig_signal before continuing */

{
    sp->counter++;
    sema_down(&sp->block);
}



void _sig_signal(sp)
     SIGNAL *sp;

/* signals one process awaiting on a SIGNAL variable that it may continue.
   If no process is waiting, then it is a NOP. */

{
    if (sp->counter) {
	sp->counter--;
	sema_up(&sp->block);
    }
}



void _sig_sigall(sp)
     SIGNAL *sp;

/* signals all process awaiting on a SIGNAL variable that it may continue.
   If no process is waiting, then it is a NOP. */

{
    if (sp->counter) {
	sp->counter = 0;
	sema_mup(&sp->block, 1000);
    }
}



void _sig_init(sp)
     SIGNAL *sp;

/* initializes a signal */

{
    sema_init(&sp->block, 0);
}



void paddr(addr)
     address *addr;

{
    if (addr) {
	char *s = ar_port((port *) addr);
	printf("%s", s);
    } else
      printf("Null address pointer");
}


void paddrn(addr)
     address *addr;

{
    paddr(addr); printf("\n");
}



static char * buf_put_real(bp, ep, val)
     char *bp;
     char *ep;
     float val;

{
    union {
	float fval;
	long ival;
    } con;

    if (!bp)
      return bp;

    con.fval = val;
    return buf_put_int32(bp, ep, con.ival);
}


static char * buf_get_real(bp, ep, val)
     char *bp;
     char *ep;
     float *val;

{
    char * status;
    union {
	float fval;
	long ival;
    } con;

    status = buf_get_int32(bp, ep,  &con.ival);
    *val = con.fval;

    return status;
}



message * _alloc_msg()
/* allocates space for a new message, from the free list if possible,
   otherwise using malloc */

{
    message *mp;

    if (msg_numfree && !msg_freelist)
      msg_fatal_error("msg_numfree > 0 & msg_freelist = NULL");

    if (msg_freelist) {
	mp = msg_freelist;
	msg_numfree--;
	msg_freelist = msg_freelist->next_msg;
    } else {
	mp = (message *)malloc(sizeof(struct message));
	if (!mp)
	  msg_fatal_error("_alloc_msg, malloc");
	msg_numalloc++;
    };

/*    bzero(mp, sizeof(struct message)); */
    mp->refcnt = 1;
    msg_inuse++;
    return mp;
}



message *msg_newmsg()
/* creates a new message, from the free list if possible, otherwise using
   malloc.  Message is tagged with the (put) capability for the local
   base_context along with the string_name of the base context.
 */

{
    message *mp = _alloc_msg();

    if (_base_context) {
	mp->hdr.h_signature = _base_context->addr; /* for completeness sake */
	mp->start_ptr =
	  mp->end_ptr =
	    mp->next_ptr = buf_put_string(mp->data + MSG_OFFSET_CTXTNAME,
					  MSG_BUF_END(mp),
					  _base_context->name);
    } else {
	mp->start_ptr =
	  mp->end_ptr =
	    mp->next_ptr = buf_put_string(mp->data + MSG_OFFSET_CTXTNAME,
					  MSG_BUF_END(mp), "no_name");
    }

    if (!mp->end_ptr)
      msg_fatal_error("msg_newmsg, buf_put");;

    return mp;
}



void msg_delete(mp)
     message *mp;

/* deletes a reference to message; if refcnt goes to zero then physically
   deletes message */

{
    if (!mp->refcnt)
      msg_fatal_error("msg_delete, deleting deleted msg");

    if (mp && (!--mp->refcnt))  {
	if (mp->ctxtp)
	  _delete_context_ref(mp->ctxtp);
	mp->next_msg = msg_freelist;
	msg_freelist = mp;
	msg_inuse--;
	msg_numfree++;
    }
}



void msg_put(mp, type, val)
     message *mp;
     int type;
     PTR val;

/* puts the value pointed to by val and of the specified type into the message
   mp */

{
    char * bp = mp->end_ptr;

    mp->end_ptr = buf_put_int16(mp->end_ptr, MSG_BUF_END(mp), (short) type);
    if (!mp->end_ptr)
      msg_fatal_error("msg_put, buf_put");

    switch (type) {
      case TYPE_INT :
	bp = buf_put_long(bp, MSG_BUF_END(mp), *(long *) val);
	break;
      case TYPE_REAL :
	bp = buf_put_real(bp, MSG_BUF_END(mp),*(float *) val);
	break;
      case TYPE_STRING :
	bp = buf_put_string(bp, MSG_BUF_END(mp), *(char **) val);
	break;
    }

    if (!bp)
      msg_fatal_error("msg_putval, buf_put");
    mp->end_ptr = bp;
}



void msg_put_int(mp, val)
     message *mp;
     int val;

/* puts an integer constant into a message.  */

{
    mp->end_ptr = buf_put_int16(mp->end_ptr, MSG_BUF_END(mp), TYPE_INT);
    mp->end_ptr = buf_put_long(mp->end_ptr, MSG_BUF_END(mp), val);
    if (!mp->end_ptr)
      msg_fatal_error("msg_put_int, buf_put");
}


#ifdef __STDC__
void msg_put_real(message *mp, float val)
#else
void msg_put_real(mp, val)
     message *mp;
     float val;
#endif

/* puts a real constant into a message.  */

{
    mp->end_ptr = buf_put_int16(mp->end_ptr, MSG_BUF_END(mp), TYPE_REAL);
    mp->end_ptr = buf_put_real(mp->end_ptr, MSG_BUF_END(mp), val);
    if (!mp->end_ptr)
      msg_fatal_error("msg_put_real, buf_put");
}


void msg_put_string(mp, val)
     message *mp;
     char *val;

/* puts an string constant into a message.  */

{
    mp->end_ptr = buf_put_int16(mp->end_ptr, MSG_BUF_END(mp), TYPE_STRING);
    mp->end_ptr = buf_put_string(mp->end_ptr, MSG_BUF_END(mp), val);
    if (!mp->end_ptr)
      msg_fatal_error("msg_put_string, buf_put");
}



void msg_put_address(mp, val)
     message *mp;
     address *val;

/* puts an address into a message.  */

{
    mp->end_ptr = buf_put_int16(mp->end_ptr, MSG_BUF_END(mp), TYPE_ADDRESS);
    mp->end_ptr = buf_put_port(mp->end_ptr, MSG_BUF_END(mp), val);
    if (!mp->end_ptr)
      msg_fatal_error("msg_put_address, buf_put");
}



int msg_get(mp, type, val)
     message *mp;
     int type;
     PTR val;

/* extracts the next element from the message, which is expected to be of the
   specified type.  The value is copied to the space pointed to by val, 
   which must be at least TYPE_MAXSIZE for this to work.  The value is stored
    in the space pointed to by val.  Note that
   for strings, the value placed in val is a pointer to the string as it
   is stored within the message.  So if the value is to persist longer
   than the message, the string value must be copied somewhere.
 */

{
    short actual_type;

    mp->next_ptr = buf_get_int16(mp->next_ptr, MSG_END(mp), &actual_type);

    if (!mp->next_ptr) {
	mp->next_ptr = MSG_END(mp);
	return -1;
    }

    if (type != actual_type) {
	fprintf(stderr,"Type mismatch, expected %s", type_to_name(type));
	printf("next = 0x%x, end = 0x%x", mp->next_ptr, MSG_END(mp));
	printf("type = %d, actual type = %d\n", type, actual_type);
	fprintf(stderr," got %s\n", type_to_name(actual_type));
	msg_printaccess(mp);
	msg_fatal_error("msg_get");
    }

    switch (type) {
      case TYPE_INT :
	mp->next_ptr = buf_get_long(mp->next_ptr, MSG_END(mp), (long *) val);
	break;
      case TYPE_REAL :
	mp->next_ptr = buf_get_real(mp->next_ptr, MSG_END(mp), (float *) val);
	break;
      case TYPE_STRING : 
	mp->next_ptr = buf_get_string(mp->next_ptr, MSG_END(mp),(char **) val);
	break;
      case TYPE_ADDRESS :
	mp->next_ptr = buf_get_port(mp->next_ptr, MSG_END(mp),(address *) val);
    }

    if (!mp->next_ptr) {
	mp->next_ptr = MSG_END(mp);
	return -1;
    } else {
	return 0;
    }
}


int msg_get_int(m, v)
     message *m;
     int *v;

{
    return msg_get(m, TYPE_INT, (PTR) v);
}


int msg_get_real(m, v)
     message *m;
     float *v;

{
    return msg_get(m, TYPE_REAL, (PTR) v);
}


int msg_get_address(m, v)
     message *m;
     address *v;

{
    return msg_get(m, TYPE_ADDRESS, (PTR) v);
}


int msg_get_string(m, v)
     message *m;
     char **v;

{
    return msg_get(m, TYPE_STRING, (PTR) v);
}



int msg_next_type(mp)
     message *mp;

/* returns the type of the next element in the message; returns -1 if at the
 * end of the message 
 */

{
    char *mnp;
    short type;

    mnp = buf_get_int16(mp->next_ptr, MSG_END(mp), &type);

    if (!mnp)
      return -1;
    else
      return type;
}



void msg_get_stamp(mp, ctxt_addr_p, seq_num_p, ctxt_name)
     message *mp;
     address **ctxt_addr_p;
     int *seq_num_p;
     char **ctxt_name;

{
    *ctxt_addr_p = &mp->hdr.h_signature;
    *seq_num_p = mp->hdr.h_offset;
    *ctxt_name = mp->data + MSG_OFFSET_CTXTNAME;
}



address *msg_sender_address(mp)
     message * mp;

{
    return &mp->hdr.h_signature;
}



/* Format of messages being sent out

   Messages when created, are tagged with address (capability) of the sender
   and the string name.

   Format of Amoeba header in sending out messages:

   port,priv = capability for object being referenced
   offset    = message sequence number
   command   = msg entry number
   size      = size of message transmitted
   extra     = options  (MSG_NO_REPLY, group forwarding stuff)
*/



int msg_rpc(addr, entry, req_mp, reply)
     address *addr;
     int entry;
     message *req_mp;
     message **reply;

/* Makes an RPC call to the server identified by the given address, sending
   req_mp destined for entry req_mp.  *reply is set to point to the reply
   message, if a reply is expected.

   Returns 0 if succeeds, -1 if fails
 */

{
    int status;

    /* record entry point adjusted with Amoeba command offset */
    MSG_ENTRY(req_mp) = entry + UNREGISTERED_FIRST_COM;
    req_mp->hdr.h_size = MSG_SIZE(req_mp);
    req_mp->hdr.h_port = *addr;
    
/*    printf("Sending message to %s, seqnum %d signature %s\n",
	   ar_port(addr), req_mp->hdr.h_offset);  *** */

    if (reply) {
	message *reply_mp = _alloc_msg();
	char *sender_name;

	req_mp->hdr.h_extra = 0;
	status = trans(&req_mp->hdr, req_mp->data, MSG_SIZE(req_mp),
		       &reply_mp->hdr, reply_mp->data, MSG_MAXSIZE);

	if (check_amoeba_error(status, "msg_rpc, trans")) {
	    msg_delete(reply_mp);
	    return ERR_CONVERT(status);
	}
	if (status && (status != reply_mp->hdr.h_size))
	  msg_fatal_error("size rcvd not equal to expected size");
	if (MSG_SEQNUM(reply_mp) != req_mp->hdr.h_offset) {
	    printf("received reply # %d for req # %d\n",
	      MSG_SEQNUM(reply_mp), req_mp->hdr.h_offset);
	    msg_fatal_error("this shouldn't happen");
	}

	if (reply_mp->hdr.h_status == (unsigned short) STD_COMBAD)
	  msg_fatal_error("msg_rpc, STD_COMBAD reply");

	if (!reply_mp->hdr.h_size)
	  msg_fatal_error("msg_rpc, reply expected but none sent");

	reply_mp->end_ptr = reply_mp->data + reply_mp->hdr.h_size;

	reply_mp->start_ptr =
	  reply_mp->next_ptr =
	    buf_get_string(reply_mp->data + MSG_OFFSET_CTXTNAME,
			   MSG_END(reply_mp), &sender_name);
	*reply = reply_mp;
    } else {
	/* No reply expected */
	header reply_hdr;
	char data[4];

	req_mp->hdr.h_extra = MSG_NO_REPLY;
	status = trans(&req_mp->hdr, req_mp->data, MSG_SIZE(req_mp),
		   &reply_hdr, data, 0);
	if (check_amoeba_error(status, "msg_rpc, trans"))
	  return ERR_CONVERT(status);
	if (status)
	  msg_fatal_error("msg_rpc, unwanted data in reply");
    }


    return 0;
}



int msg_rpc_reply(req_mp, reply_mp)
     message *req_mp;
     message *reply_mp;

/* sends reply to RPC message */

{

/*    printf("msg_rpc_reply, replying to message 0x%x, %d 0x%x %d\n",
	   req_mp, MSG_SEQNUM(req_mp), MSG_OPTIONS(req_mp),
	   MSG_OPTIONS(req_mp) & MSG_NO_REPLY);  *** */

    /* make sure that we are sending a reply iff one is expected.  However,
       an Amoeba RPC always requires a reply. If no reply is expected,
       then reply_mp should be NULL */

    if (MSG_OPTIONS(req_mp) & MSG_NO_REPLY) {
	if (reply_mp) {
#ifdef DEBUG
	    printf("request_msg:  ");
	    msg_printaccess(req_mp);
	    printf("reply_msg:  ");
	    msg_printaccess(reply_mp);
#endif
	    msg_fatal_error("msg_rpc_reply, unwanted reply");
	} else
	  reply_mp = _empty_msg;
    } else {
	if (!reply_mp)
	  msg_fatal_error("msg_rpc_reply, no reply");
    }

    reply_mp->hdr.h_port = req_mp->hdr.h_port;
    reply_mp->hdr.h_size = reply_mp->end_ptr - reply_mp->data;
    reply_mp->hdr.h_status = 0;
    /* put as seq number the seq number of the request */
    MSG_SEQNUM(reply_mp) = MSG_SEQNUM(req_mp);

    /* record the last (non-null) reply sent out.  Note that RPC's not
       requiring a reply are checked first.  Should we rcv a duplicate
       request, as long the request is consistent wrt needing a reply or not,
       this mechanism will work.  We only do this work if a base_context has
       been defined. */

    if (_base_context && (req_mp->ctxtp != _base_context)) {
	/* we don't keep track of the last reply sent to ourself.

	   A problem arises if we do, because the msg_* routines are used
	   to effect the state transfer.  This can result in some replicas
	   having a last reply for the base_context, with others not having
	   such a reply.

	   Essentially this means that a process can not make an RPC request
	   fault-tolerantly to its own context.  State machines should not be
	   making RPC's to themselves in the first place.

	   In a rewrite of this code, all the context handling info should be
	   done at a higher level, not at the msg_ level. */

	msg_increfcount(reply_mp);
	if (req_mp->ctxtp->last_reply)
	  msg_delete(req_mp->ctxtp->last_reply);

	req_mp->ctxtp->last_reply = reply_mp;
	req_mp->ctxtp->last_replied_seqnum = MSG_SEQNUM(req_mp);
	_sig_sigall(&req_mp->ctxtp->new_reply);
    }

    if (MSG_OPTIONS(req_mp) & MSG_REQ_FORWARD) {

	extern int _my_rep_grpid;

	if (MSG_GRP_SENDER_ID(req_mp) == _my_rep_grpid) {
	    /* strictly speaking, we don't really need to bump up here
	       (and down in msg_grp_forward) the refcount, since the
	       message will remain saved as the last reply---the client
	       can't make a new request, overwriting that reply, until
	       it has gotten this message */

	    msg_increfcount(reply_mp);
	    req_mp->grp_forwarded_mp->grp_reply_mp = reply_mp;
	    sema_up(&req_mp->grp_forwarded_mp->grp_sync);
	}

    } else {
	putrep(&reply_mp->hdr, reply_mp->data, MSG_SIZE(reply_mp));
    }

    return 0;
}



message * msg_rpc_rcv(rcv_port)
     address *rcv_port;

/* returns next message from rcv_port */

{
    message *mp = _alloc_msg();
    int status;
    char *sender_name;		/* dummy, since sender equals mp->data */

/***    printf("calling getreq 0x%x, 0x%x,", &mp->hdr, mp->data);
    printf("listening to port %s\n", ar_port((port *) rcv_port)); */

    for (;;) {
	register int cmd;

	mp->hdr.h_port = *rcv_port;

	status = getreq(&mp->hdr, mp->data, MSG_MAXSIZE);
	check_amoeba_error(status, "msg_rpc_rcv, get_req");
	cmd = MSG_ENTRY(mp);

	if (cmd < STD_LAST_COM) {
	    mp->data[0] = (char) 0;
	    switch (cmd)  {
	      case STD_INFO :
		{
		    extern GROUP *_replicated_grp;
		    strcpy(mp->data, "msg server, x replicas ");
		    mp->data[12] = '0' + _replicated_grp->grp_state.st_total;
		}
	      case STD_TOUCH :
		mp->hdr.h_status = STD_OK;
		mp->hdr.h_size = strlen(mp->data);
		putrep(&mp->hdr, mp->data, strlen(mp->data));
		break;
	      case STD_DESTROY :
		mp->hdr.h_status = STD_OK;
		putrep(&mp->hdr, mp->data, 0);

		{
		    extern int _replicated_mode;
		    extern GROUP *_replicated_grp;
		    if (_replicated_mode) {
			msg_grp_send(_replicated_grp, STD_DESTROY, _empty_msg);
			break;
		    } else {
			extern char *program_name;
			printf("process %s received STD_DESTROY!\n",
			       program_name);
			exitprocess(0);
		    }
		}

	      default : 
		mp->hdr.h_status = STD_COMBAD;
		putrep(&mp->hdr, mp->data, 0);
		break;
	    }

	    continue;
	}


	if ((cmd < UNREGISTERED_FIRST_COM) ||
	    (cmd >= MAX_MSG_ENTRIES + UNREGISTERED_FIRST_COM)) {
	    printf("Bad msg received, header %d\n", cmd);
	    mp->hdr.h_status = STD_COMBAD;
	    putrep(&mp->hdr, mp->data, 0);
	    continue;
	}

	break;
    }

    /* subtract Amoeba command offset */
    mp->hdr.h_command -= UNREGISTERED_FIRST_COM;

    if (status != mp->hdr.h_size)
      msg_fatal_error("msg_rpc_rcv, rcvd size not equal to transmitted size");

    mp->end_ptr = mp->data + mp->hdr.h_size;
    mp->start_ptr =
      mp->next_ptr = buf_get_string(mp->data + MSG_OFFSET_CTXTNAME,
				    MSG_END(mp),&sender_name);

    mp->ctxtp = ctx_lookup_context(sender_name);

    if (!mp->start_ptr)
      msg_fatal_error("msg_rpc_rcv, buf_get");

#ifdef DEBUG
    if (msg_trace & TRACE_MSGS)
      printf("rcvd msg from %s seqnum %d for entry %d [0x%x] size %d\n",
	     MSG_SENDER_NAME(mp), mp->hdr.h_offset, mp->hdr.h_command,
	     mp->hdr.h_extra, MSG_SIZE(mp));
#endif

/***    printf("msg_rpc_rcv, size  %d\n", MSG_SIZE(mp)); */

    return mp;
}



void msg_printaccess(mp)
     message *mp;

/* prints out format of message */

{
    short type;
    char *next_ptr = mp->next_ptr;
    static char * msg_err = "Ill-formed message, no value!\n";

    msg_rewind(mp);
    printf("message 0x%x, %d ref%s, entry %d, size %d, sender %s\n",
	   mp, mp->refcnt, (mp->refcnt != 1) ? "s" : "",
	   mp->hdr.h_command, MSG_SIZE(mp), MSG_SENDER_NAME(mp));

    /* first get type, then get value */

    type = msg_next_type(mp);

    while (type >= 0) {
	printf("%s: ", type_to_name(type));
	switch (type) {
	  case TYPE_INT : 
	    {
		int value;
		if (msg_get_int(mp, &value)) {
		    printf(msg_err); return;
		}
		printf("%d\n", value);
		break;
	    }
	  case TYPE_REAL :
	    {
		float value;
		if (msg_get_real(mp, &value)) {
		    printf(msg_err); return;
		}
		printf("%f\n", value);
		break;
	    }
	  case TYPE_STRING :
	    {
		char *value;
		if (msg_get_string(mp, &value)) {
		    printf(msg_err); return;
		}
		printf("%s\n", value);
		break;
	    }
	  case TYPE_ADDRESS :
	    {
		port value;
		if (msg_get_address(mp, &value)) {
		    printf(msg_err); return;
		}
		printf("%s\n", ar_port(&value));
		break;
	    }
	}

	type = msg_next_type(mp);
    }

    /* reset next_ptr to where it originally was */
    mp->next_ptr = next_ptr;
}



/* We dislike the Amoeba thread mechanism, w/ its malloc interface.  Rather
 *  than live with it, we make it look like the ISIS one.
 *
 * NB:  We only allow one pending request per context (program).  In particular
 * parallel threads may not concurrently invoke RPC's.  This restriction comes
 * about because we keep track of the last reply on per context basis; to
 * remove this restriction, we would have to essentially consider each
 * requesting thread to be a context.  Identically functioning threads in
 * different replicas would have to be identified identically.
 * 
 * Note also that we currently do not support preemptive thread scheduling,
 * as that destroys deterministic execution ...
 *
 */


struct thread_struct {
    PTR parm;
    void (*func) _ARGS((void *));
    char *name;
};


static void thread_invoke(p, size)
     char *p;
     int size;

{
    struct thread_struct *tsp;

    tsp = (struct thread_struct *) p;
#ifdef DEBUG
    if (msg_trace & TRACE_THREADS)
      printf("thread_invoke calling new thread %s\n", tsp->name);
#endif

    (*tsp->func)(tsp->parm);

#ifdef DEBUG
    if (msg_trace & TRACE_THREADS)
      printf("thread_invoke exiting thread %s\n", tsp->name);
#endif
}



int _t_fork(func, parm, name)
     void (*func) _ARGS((void *));
     void *parm;
     char *name;

/* forks off a new thread running the specified function, with parm passed
   as a parameter (name given for debugging) */

{

#ifdef KERNEL
    extern int kernelprocess;

    NewThread(kernelprocess, func, parm, MSG_STACKSIZE);

#else

    struct thread_struct *tsp;

    tsp = (struct thread_struct *)malloc(sizeof(struct thread_struct));

    if (!tsp)
      msg_fatal_error("t_fork, malloc");

    tsp->parm = parm;
    tsp->func = func;
    tsp->name = name;

    return thread_newthread(thread_invoke, MSG_STACKSIZE, (char *) tsp,
			    sizeof(struct thread_struct));
#endif

}
