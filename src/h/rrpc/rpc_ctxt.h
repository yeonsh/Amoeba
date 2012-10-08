/*	@(#)rpc_ctxt.h	1.1	93/10/07 17:03:59 */
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
 * Context-related includes
 */

/* context definitions */

struct contextnode {
    struct contextnode 
      *next_context;		/* next in global list of linked ctxts */
    struct contextnode
      *prev_context;		/* previous in global list of linked ctxts */
    int refcnt;		/* number of references given to this ctxt */
    char *name;			/* name of context */
    address addr;		/* the address for the context */
    int local;			/* TRUE if context is local (base_context) */
    struct contextnode *next;	/* next pointer in hash linked list */
    struct contextnode *prev;	/* prev pointer in hash linked list */
    int addr_lookedup;		/* TRUE if address has been looked up */
    long last_seqnum;		/* last seq num we've heard from this ctxt */
    long last_replied_seqnum;	/* seqnum of last request we've replied to */
    message *last_reply;	/* last reply we gave to this ctxt */
    SIGNAL new_reply;		/* signal variable indicating reply avail */
    semaphore vsync_sem;	/* for virt. sync; see check_seqnum */
    port get_port;		/* if local, the get_port */
    port check_field;		/* if local, cap. check field (not used) */
    int watch_count;		/* number of requests to watch ctxt */
    int monitoring;		/* TRUE if currently monitoring */
    int is_null;		/* TRUE if address is null.  This exists
				   as a convenience.  The address may be
				   updated but should still be considered
				   as being null (for the sake of virtual
				   synchrony) until context_alive has run */
};


typedef struct contextnode CTXT_NODE;

#define MAX_CTXT_LIST 128

struct ctxt_list {
    int size;
    CTXT_NODE *ctxt[MAX_CTXT_LIST];
};

typedef struct ctxt_list CTXT_LIST;

CTXT_NODE *_base_context;

#define GLBL_CTXT NULL
#define IS_GLBL_CTXT(c) (!(c))


CTXT_NODE * ctx_lookup_context _ARGS((char *context_name));

int ctx_rpc _ARGS((CTXT_NODE *ctxtp, int entry, message *req_mp,
		   message **reply));

typedef message *(*msg_state_xfer_routine_t) _ARGS((int));
typedef int (*msg_state_rcv_routine_t) _ARGS((int, message *));

CTXT_NODE * ctx_define_context _ARGS((
		       char *context_name,
		       int is_replicated,
		       int num_state_domains,
		       message *(*state_xfer_routine[])(int),
		       int (*state_rcv_routine[])(int, message *)
		       ));

#define MAX_DOMS 3
