/*	@(#)flrpc.h	1.1	96/02/27 10:39:41 */
#ifndef __FLRPC_H__
#define	__FLRPC_H__

/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define MAX_RPC_RETRIAL	10	/* max number of retrials */
#define LOCRETRIAL	1	/* number of retrials for a given locate dist */
#define FIRSTLOCATE	5	/* when to start to locate */
#ifdef FLIPIP_DEBUG
#define MAXREASSEMBLE	60000	/* reassemble timer (ms) */
#else
#define MAXREASSEMBLE	4000	/* reassemble timer (ms) */
#endif
#define	SERVETIMEOUT	1000	/* time before asking the server if got the */
				/* request (ms) */
#define REPLYTIMEOUT(n) (n << 4)
#define CRASHTIME	20000	/* alive timer (ms) */
#define LOCTIMEOUT	100	/* interval between sending next loc pkt (ms) */
#define SWEEPINTERVAL 	10	/* time between two sweeps (ms) */
#define MAXSERVERREPLY	30000	/* max value for the reply timer of server */


/* Possible states of rpc protocol.
 */
#define IDLE		0	/* F_DOOP, F_SIGTRANS */
#define LOCATE		1	/* F_SIGNAL */
#define PUTREQ		2	/* F_REASSEM, F_UNREACHABLE, F_NACKED,
				 * F_SIGNAL, F_UNTRUSTED
				 */
				/* F_RETRANS */
#define GETREQ		3	/* F_REASSEM */
#define PUTREP		4	/* F_NACKED */
#define GETREP		5	/* F_REASSEM, F_SIGNAL */

/* Protocol flags */
#define F_DOOP		0x1	
#define F_NACKED	0x2
#define F_FORWARD	0x4
#define F_REASSEM	0x8
#define F_SIGNAL	0x10

#define F_RETRANS	0x20
#define F_UNREACHABLE  	0x40
#define F_SIGTRANS	0x80
#define F_UNTRUSTED	0x100

/* Possible message types for rpc protocol. */
#define RPC_LOCATE	 1
#define RPC_HEREIS	 2
#define RPC_REQUEST	 3
#define RPC_REPLY	 4
#define RPC_ACK		 5
#define RPC_NAK		 6
#define RPC_ENQUIRE	 7
#define RPC_ALIVE	 8
#define RPC_RECEIVED	 9
#define RPC_FORWARD	10
#define RPC_FAIL	11

/* Possible states of a entry in the state table. */
#define ST_FREE		1
#define ST_IDLE		2

/* Possible flags in the protocol header. */
#define FL_BIGENDIAN	0x1
#define FL_RETRANS	0x2
#define FL_REQUEST	0x4
#define FL_REPLY	0x8
#define FL_SIGTRANS	0x10

#define MIN(a,b)	((a < b) ? a : b)

#ifndef UNIX

#ifndef NEW_SEGMENTS
#define uunmap(t, a, s) 
#endif /* NEW_SEGMENTS */

#endif /* UNIX */

/* Macro to convert thread(-index) to pointer. */
#define RPC_THREADSTATE(t)      (statetab + THREADSLOT(t))
#define RPC_STATE(p, t)    	(p->ps_thread[t])


/* Amoeba rpc protocol header. */
typedef struct amphdr {
    adr_t	ah_kid;		/* kernel identifier */
    port	ah_port;	/* port for RPC_REQUEST, etc. */
    uint8	ah_type;	/* see above */
    uint8	ah_flags;	/* see above */
    uint32	ah_tid;		/* transaction identifier */
    uint16	ah_dest;	/* destination state entry */
    uint16	ah_from;	/* source state entry */
} amphdr_t, *amphdr_p;


typedef struct state state_t, *state_p;

/* Protocol state per process. */
typedef struct procstate {
    int			ps_init;	/* is this entry used? */
    state_p		*ps_thread; 	/* threads of this process */
    adr_t		ps_myaddr; 	/* FLIP address of process */
    adr_t		ps_myprvaddr; 	/* private address */
    uint16		ps_nthread; 	/* number of threads in this process */
    state_p 		ps_lastst;	/* last entry used to reassemble */
    int			ps_ep;		/* FLIP EP */
    int			ps_bcastep;	/* flip end point for broadcast */
    int			ps_ifno;	/* interface number */
    int			ps_pid;		/* process pid */
} procstate_t, *procstate_p;


/* Protocol state is stored in the following structure. Each threads has its
 * own state entry. However, threads in the same process share a flip address.
 */
struct state {
    thread_t		*st_thread; 	/* pointer to thread table */
    procstate_p		st_proc; 	/* pointer to protocol process state */
    uint16		st_index;	/* index in statetab */
    int			st_state;	/* protocol state */
    uint32		st_flag;	/* protocol flags */
    interval 		st_ltime;	/* locate timer */
    int			st_deltatime;	/* time between sending next one */
    int			st_timeout;	
    int			st_retrial;	/* number of retrials to go */
    uint32		st_mytid;	/* next transaction id to use */
    int			st_reassemble;	/* reassemble timer */
    port		st_pubport; 	/* the servers public port */
    int			st_security; 	/* 0 or FLIP_SECURITY */
    /* server info about client*/
    uint32		st_ctid;	/* clients tid */
    adr_t		st_caddr;	/* clients flip address */
    uint16		st_cident;	/* state entry for client */
    adr_t		st_ckid;	/* kernel id for client */
    int 		st_crtime; 	/* reply timer */
    /* client info about server */
    port		st_sport; 	/* the servers port */
    uint16		st_sident;	/* server's state entry */
    adr_t		st_saddr;	/* servers address */
    interval 		st_srtime; 	/* reply timer */
    portcache_p		st_cache;	/* last used port */
    pkt_p		st_ackpkt; 	/* ack packet */
    /* send info */
    header		*st_shdr;	/* outgoing header */
    char 		*st_sdata;
    f_size_t		st_scnt;
    /* receive info */
    header		*st_rhdr;	/* incomming header */
    char 		*st_rdata;
    f_size_t		st_rcnt;	/* size of incomming message */
    f_size_t		st_roffset;	/* next byte of incomming message */
    f_msgcnt_t 		st_rmessid;	/* message id of incomming message */
    procstate_p		st_local_client;/* local client, if set */
};

/* Is the thread acting as a client or as a server? */
#define RPC_ACTIVE(st)	((st)->st_state != IDLE || (st)->st_flag != 0)

/* Is the thread acting as a server? */
#define SERVER(st) 	(((st)->st_flag & F_DOOP) || ((st)->st_state == GETREQ)\
			 || (st)->st_state == PUTREP)

/* Is the thread acting as a client? */
#define CLIENT(st)	((st)->st_state == PUTREQ || (st)->st_state == GETREP\
			 || (st)->st_state == LOCATE)

/* Is the thread serving a client? */
#define SERVING(st)	(((st)->st_flag & F_DOOP) || (st)->st_state == PUTREP)

/* Is the thread performing an operation for the client? */
#define DOOPERATION(st) (((st)->st_flag & F_DOOP) && (st)->st_state == IDLE)

/* Is the thread not acting as a client? */
#define NOCLIENT(st)	((st)->st_state == IDLE && ((st)->st_flag == 0 ||\
						  (st)->st_flag & F_DOOP))

/* Is the thread waiting for a reply from the server? */
#define WAITREPLY(st)	((st)->st_state == PUTREQ || (st)->st_state == GETREP)

/* Is the thread in a state when reassembling is possible? */
#define REASSEMBLE(st)	(WAITREPLY(st) || (st)->st_state == GETREQ)

#define MAKE_IDLE(st)	do { (st)->st_state = IDLE; \
			     (st)->st_timeout = -1; } while (0)

#endif /* __FLRPC_H__ */
