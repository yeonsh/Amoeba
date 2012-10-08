/*	@(#)rpc_group.h	1.1	93/10/07 17:04:05 */
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
 */

#include <group.h>

#define MAX_GROUP_SIZE 8
#define MAX_XFER_DOMAINS 4

#define JOIN_INHIBIT (MAX_XFER_DOMAINS + 1)


struct msg_grp_entry {
    void (*func)();		/* function, NULL if queued */
    char *name;			/* string name of entry */
};

enum group_states {JOINING, JOINED};

struct msg_group {
    int grp_handle;		/* handle used for Amoeba group calls */
    port grp_port;		/* port associated with group */
    char *grp_name;		/* the group's name as a string */
    grpstate_t grp_state;	/* Amoeba group state */
    g_indx_t 
      grp_mem_id[MAX_GROUP_SIZE]; /* list of Amoeba group ids */

    /* routine to be called when ever group's (active) membership changes */
    void (*grp_monitor_routine) _ARGS((struct msg_group *, void *));

    PTR grp_monitor_arg;	/* arg to be passed when calling above */

    /* The state transfer stuff */
    int grp_inhibit_joins;	/* TRUE if joins not currently allowed */
    int grp_active;		/* TRUE if not yet full-fledged member, i.e, */
				/* grp_join done but awaiting state  */

    int grp_num_domains;	/* number of state transfer domains */

    /* group called to receive the state */
    int (*grp_state_rcv_routine[MAX_XFER_DOMAINS]) _ARGS((int, message *));

    /* group called to form a message containing the state */
    message *(*grp_state_xfer_routine[MAX_XFER_DOMAINS]) _ARGS((int));

    address grp_xfer_gaddr;	/* RPC get port for receiving group state */
    address grp_xfer_paddr;	/* RPC put port for receiving group state */
    SIGNAL grp_state_wait;	/* SIGNAL variable the msg_grp_join routine */
				/* waits on while awaiting the state */

    struct msg_grp_entry grp_entry_table[MAX_MSG_ENTRIES];
				/* the entry points to be called upon */
				/* message receipt */
};


typedef struct msg_group GROUP;


#define GRP_FIRST_COMMAND 100

#define GRP_JOIN_REQ   100
#define GRP_RESET      102
#define GRP_LEAVE      103
#define GRP_CREATE     104
#define GRP_JOIN_ABORT 105

typedef void (*msg_grp_monitor_routine_t) _ARGS((GROUP *gp, void *dummy));

GROUP *msg_grp_join _ARGS((char *gname, int options,
		     int num_domains,
		     message *(*grp_state_xfer_routine[])(int),
		     int (*grp_state_rcv_routine[])(int, message *),
		     void (*monitor_routine)(GROUP *gp, void *dummy),
		     void *arg));

void msg_grp_leave _ARGS((GROUP *gp));
message *msg_grp_receive _ARGS((GROUP *gp));
int msg_grp_send _ARGS((GROUP *gp, int entry, message *msg));
void msg_define_grp_entry _ARGS((GROUP *gp, int entry, void (*func)(message *), char *entry_name));
void msg_grp_listener _ARGS((GROUP *grp));

#define GRP_NUM_MEMBERS(gp) ((gp)->grp_state.st_total)
#define GRP_MY_ID(gp) ((gp)->grp_state.st_myid)
#define I_AM_COORDINATOR(gp) ((gp)->grp_state.st_seqid == GRP_MY_ID(gp))
#define MSG_GRP_SENDER_ID(mp) ((mp)->hdr.h_extra & 0xff)

#define MSG_GRP_MANUAL_RCV 1
#define MSG_GRP_RESILIENT  2

#define msg_grp_num_members(gp) GRP_NUM_MEMBERS(gp)

