/*	@(#)flgrp_type.h	1.7	96/02/27 14:02:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FLGRP_TYPE_H__
#define __FLGRP_TYPE_H__

#define FL_RECEIVING		0x1
#define FL_SENDING		0x2
#define FL_JOINING		0x4
#define FL_RESET		0x8
#define FL_VOTE			0x10
#define FL_RESULT		0x20
#define FL_COHORT		0x40
#define FL_COORDINATOR		0x80
#define FL_SYNC			0x100
#define FL_LOCATING		0x200
#define FL_DESTROY		0x400
#define FL_NOUSER		0x800
#define FL_FAILED		0x1000
#define FL_QUEUE		0x2000
#define FL_SIGNAL		0x4000
#define FL_COLLECTING		0x8000

#define STOP    (-1)
#define ABORT   (-2)
#define CRASHED (-3)
#define SILENT  (-4)

#define MIN(a,b)        ((a<b) ? a : b)
#define MAX(a,b)        ((a<b) ? b : a)

extern int grp_debug_level;
#ifndef GRP_NDEBUG
#define GDEBUG(level, stmt)     if (level <= grp_debug_level) { stmt; }
#else
#define GDEBUG(level, stmt)
#endif


typedef struct barrier {
	long		b_barrier;
	struct barrier 	*b_next;
} barrier_t, *barrier_p;



/* Possible values for the field m_state: */
#define M_UNUSED	0
#define M_MEMBER	1
#define M_JOINING	2
#define M_LEAVING	3
#define M_LEFTMEMBER	4 /* the sequencer node after its local member left */

#define IS_MEMBER(state) (((state) == M_MEMBER) || ((state) == M_LEFTMEMBER))
#define IS_LIVINGMEMBER(state) ((state) == M_MEMBER)

#include "flgrp_malloc.h"

typedef struct member {
        /* general info about member */
	uint32		m_state;		/* state of member */
	adr_t 		m_addr;			/* address of member */
	adr_t		m_rpcaddr;		/* address of rpc entity */
	int 		m_ep;			/* member EP */
	/* variables for bcast protocol */
	g_index_t	m_nack;			/* number of ack's */
	g_seqcnt_t	m_expect;		/* seqno expected by member */
	g_msgcnt_t	m_messid;		/* next messid to use */
	int		m_retrial;		/* retrial counter */
	int 		m_vote;			/* vote for this member */
	int		m_replied;		/* has the member replied ? */
	/* variables to marshall flip messages */
	bchdr_t         m_bc;
	char		*m_data;
	f_msgcnt_t	m_rmid;
	f_size_t	m_roffset;
	f_size_t	m_total;
	/* variables to buffer a message (method 2) */
	bchdr_t 	m_bufbc;
	char 		*m_bufdata;
	f_size_t        m_bufsize;
	int		m_bigendian;
} member_t, *member_p;


typedef struct {
	int         g_used;		/* group state */
	int         g_ifno;		/* group ntw */
	port        g_port;		/* a port identifies a group */
	int         g_pid;

	/* Synchronization variables */
	char        g_sender;			
	char        g_receiver;	      
	char        g_reset;

	/* General group info */
	uint32      g_flags;		/* flags */
	f_size_t    g_large;		/* use PB Method or BB Method? */
	int         g_maxretrial;
	uint16      g_nbuf;		/* total number of buffers */
	Mem_descr   g_mem_descr;	/* descriptor for the buf allocator */
	g_index_t   g_maxgroup;		/* maximum size of the group */
	uint16      g_logsizemess;	/* log size of a message */
	f_size_t    g_maxsizemess;	/* maximum size of a message */
	f_size_t    g_maxctrlsize;	/* maximum size of a control message */
	g_index_t   g_resilience;	/* degree of resilience */
	g_incarno_t g_incarnation;	/* incarnation number */
        g_index_t   g_total;		/* group size */
        g_index_t   g_total_noseq;	/* group size without sequencer */
	member_p    g_me;		/* my id */
	g_index_t   g_index;
	member_p    g_seq;		/* sequencer id */
	g_index_t   g_seqid;
	g_index_t   g_memrank;		/* member rank */
	member_p    g_coord;		/* coordinator id */
	member_p    g_last_sender;	/* last member sending data */
	adr_t       g_addr;		/* group address */
	adr_t       g_prvaddr;		/* private version g_addr */
	adr_t       g_addr_all;		/* all members listen to this one */
	adr_t       g_prvaddr_all;	/* private version */
	int         g_bcastep;		/* EP for broadcast address */
	int         g_ep;		/* group EP */
	int         g_ep_all;		/* all EP */
	member_p    g_member;		/* member info */

	/* Buffer info */
	uint16      g_hstnbuf;
        uint16      g_memnbuf;
	uint16      g_bufnbuf;

	/* History info */
	hist_p      g_history;		/* history of bcast messages */
	uint16      g_nhist;		/* size of the used history */
	uint16      g_nloghist;		/* 2 log size of the history */
	uint32      g_hstmask;		/* % hist mask */
	uint16      g_nhstfull;		/* when is hist full? */
	g_seqcnt_t  g_nextseqno;	/* next to use sequence number */
	g_seqcnt_t  g_minhistory;	/* lowest entry used */
	g_seqcnt_t  g_nexthistory;	/* next entry to store message */
	uint16      g_silent;		/* quiet time */
	uint16      g_maxsilent;	/* max quiet time */
	uint16      g_soonsilent;	/* how soon considered silent? */

	/* Retransmission status */
	g_seqcnt_t  g_retrans_seq;	/* seqno of last retrans asked */
	int	    g_retrans_pending;	/* to avoid asking same msg twice */
	uint32      g_retrans_time;	/* time of last retrans request */

	/* Timers. */
	interval    g_atimeout;		/* alive */
	interval    g_stimeout;		/* sync */
	interval    g_rtimeout;		/* reply */
	interval    g_ltimeout;		/* locate */
	interval    g_resettimeout;	/* reset */
	interval    g_replytimer;	/* timer on a request */
	interval    g_synctimer;	/* timer to synchronize the group */
	interval    g_alivetimer;	/* timer to check the group */
	interval    g_resettimer;

	/* Administration for group interface */
	barrier_p   g_fbarrier;		/* list of blocked senders */
	barrier_p  *g_lbarrier;

#ifndef NO_CONGESTION_CONTROL
	/* Variables used to estimate the state of network congestion.
	 * If congestion is suspected, we decrease the rate at which
	 * we submit new requests to the FLIP layer.  Note that ideally
	 * a congestion control layer should be implemented within FLIP
	 * itself (e.g. a Rate Control mechanism), but that would be a
	 * whole lot more work.
	 */
	/* roundtrip statistics: */
	interval    g_cng_min_roundtrip; /* min roundtrip during this epoch */
	interval    g_cng_max_roundtrip; /* max roundtrip during this epoch */
	interval    g_cng_avg_roundtrip; /* average roundtrip               */
	interval    g_cng_sdv_roundtrip; /* standard deviation of roundtrip */
	interval    g_cng_retrans;       /* derived retransmit timer value  */
	interval    g_cng_timer;	 /* timer for congestion module     */
	interval    g_cng_timeout;	 /* timeout value		    */
	/* rate control state variables: */
	uint32	    g_cng_cur_rate;	 /* current rate in Kb/sec          */
	uint32	    g_cng_thresh_rate;	 /* cong. avoidance threshold rate  */
	/* state variables used to implement this rate (clumsily): */
	uint32      g_cng_start_slice;	 /* start of current time slice     */
	uint32      g_cng_start_send;	 /* time we started sending last msg*/
	uint32	    g_cng_sent;	 	 /* Kb sent in current timeslice    */
	uint32	    g_cng_tot_sent;	 /* Kb sent in total                */
#endif /* NO_CONGESTION_CONTROL */

} group_t, *group_p;

#endif /* __FLGRP_TYPE_H__ */
