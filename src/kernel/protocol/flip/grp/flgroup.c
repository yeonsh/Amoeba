/*	@(#)flgroup.c	1.21	96/02/27 14:00:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h" 
#include "stderr.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "string.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "group.h"
#include "flgrp_header.h"
#include "flgrp_hist.h"
#include "flgrp_type.h"
#include "flgrp_member.h"
#include "flgrp_alloc.h"
#include "flgrp_marsh.h"
#include "flgrp_hstpro.h"
#include "sys/flip/flip_proto.h"
#include "groupvar.h"
#include "module/fliprand.h"

#include "machdep.h"
#include "global.h"
#include "exception.h"
#include "kthread.h"
#include "sys/flip/group.h"

#include "byteorder.h"
#include "grp_endian.h"
#include "../rpc/am_endian.h"
#include "sys/proto.h"
#include "sys/flip/measure.h"


/*
 *  This module implements group communication. It exports seven functions:
 * 	- grp_create()
 *	- grp_join()
 *	- grp_leave()
 *	- grp_send()
 *	- grp_receive()
 *	- grp_info()
 *	- grp_set()
 *
 * If you are unfamiliar with the protocol, please read one of the group
 * communication papers and the manual page.
 *
 * The group communication can be implemented in two ways. In method PB a
 * point-to-point message is sent to the sequencer and the sequencer 
 * broadcasts the message to the group. In method BB a member sends a broadcast
 * message to the group and the sequencer broadcasts an accept-message.
 * This module implements and uses both, possibly intermixed. 
 * The method used is determined by the variable g_large. If a message is 
 * larger than the value stored in Large, method BB is used. Otherwise, 
 * method PB is used.
 *
 * The code also implements a protocol to recover from processor failures.
 * If g_resilience degree > 0, the protocol guarantees that all members will
 * receive all message in the same order, even if g_resilience kernels fail.
 * For a resilience degree > 0 the protocol is more complicated. First, a 
 * point-to-point message is sent to the sequencer. The sequencer allocates
 * a new sequencer number and multicasts the message to the group. This message
 * is not the official accept message; it still has the type BC_BCASTREQ. 
 * After resilience degree kernels have received the message, stored it 
 * tentatively in their history, and sent an ack to the sequencer, then the
 * sequencer accepts the message officially. It sends a short accept message
 * to the group telling that the message is now officially accepted and can
 * be delivered to the user application. Thus, this protocol requires 1
 * point-to-point msg, one multicast, r short acks, and 1 short multicast.
 * The code implements this protocol and also the variant for method BB.
 * (Again, g_large determines which variant is used).
 *
 * At first sight, it may seem that the protocol for a resilience degree > 0
 * could do with less messages. For example, a member broadcasts the message
 * to all members, resilience kernels send an ack, and the sequencer broadcasts
 * the accept message. This requires one broadcast, resilience acks, and one
 * short broadcast. However, this protocol is not correct. Consider the
 * following scenario. The sequencer accepts a number of messages officially,
 * because resilience kernels have sent an ack, and the broadcast from the
 * sequencer announcing this event gets lost and the sequencer delivers the
 * message to its applications. 
 *
 * Now, the sequencer crashes and the recovery protocol is started. The 
 * remaining members have the messages buffered, but they do not
 * know in which order the sequencer has delivered the message to its
 * application! Thus, after recovery, they may deliver the message in a
 * a different order than the sequencer, which can result in havoc. For example,
 * consider a reliable storage service with n servers, each with their own
 * disk. The storage serivice fully replicates the storage on each server.
 * If each server uses the messages to update replicas on disk and the remaining
 * servers update them in a different order than the server on the failed
 * sequencer. The replicas become inconsistent. If the ex-sequencer machine
 * comes back up, it may return a different reply to a client than the other
 * servers.
 *
 * Three important datastructures are used to implement the group
 * communication: group, member, and history. Group_t contains general
 * information about the group, such as group addresses timers, list of 
 * members (processes), and pointers into the history, etc. For each member
 * of a group running on this kernel, a structure group_t is maintained.
 * For each member in the group there is a structure member_t. It contains
 * info about the member, such the next message that the member is expecting,
 * the member's address, etc..
 *
 * The history is a circular buffer in which messages are stored in the order
 * of their sequence numbers. It consists of three parts. The first part are 
 * messages that are accepted in the global order (messages between
 * g_minhistory and g_nextseqno). The second part are messages that are not 
 * accepted yet. As soons as the sequencer finishes the synchronization, they
 * will be accepted (message between g_nextseqno and g_nexthistory).
 * This part is for members always empty. The third part is empty for the
 * sequencer but is used by members to buffer out of order messages. These
 * messages can only be delivered to the application if the preceding messages
 * have been received.
 *
 * The following picture depicts the relation between the structures group,
 * member, and history. The picture shows two members of G1, and one member of
 * G2, all running on this kernel.

M1 of G1               	   M2 of G1                  M1 of G2               
|----|    members of G1	   |----|    members of G1   |----|    members of G2
| M1 |   ------------- 	   | M2 |   -------------    | M1 |   ------------- 
| G1 |-->|M0|M1|...|Mn|	   | G1 |-->|M0|M1|...|Mn|   | G2 |-->|M0|M1|...|Mn|
|    |   ------------- 	   |    |   -------------    |    |   ------------- 
|----|                 	   |----|                    |----|                 
  |                    	     |                         |                    
  |                    	     |                         |                    
 \ /                   	    \ /                       \ /                   
---------------------- 	   ----------------------    ---------------------- 
|history of G1 for M1 |	   |history of G1 for M2 |   |history of G2 for M1 |
---------------------- 	   ----------------------    ---------------------- 

 */


/* TODO:
 * 1) Timer values should be dynamically determined.
 * 2) Recovery problems:
 *    - test multiple + failed resets more.
 *    - during recovery message are sometimes accepted that should not
 *      be accepted. A sequencer is dead, the group starts to reform, sequencer
 *	comes suddenly to alive and broadcasts a BC_BCAST.
 */


extern uint16	grp_npkt; 	/* number of packets used for this module */
extern uint16   grp_pktsize;	/* size of direct data in grp packet. */
extern uint16	grp_nbarrier;	/* Number of barriers */

/* debug levels */
#define LR	0	/* always printed */
#define LM	1	/* for rare events/debugging */
#define LF	2	/* printfs for each message sent/received */
#define LB	3	/* very verbose */

#define BCLARGE		10000 	/* default values */
#define MINLOGMSG	10	/* log(smallest allowable message size) */
#define MINNBUF		4	/* min number of buffers to run the protocol. */
#define MAXRESILIENCE	32	/* maximum resilience degree. */

#define ALIVETIMEOUT	10000	/* default timer values; all in ms. */
#define SYNCTIMEOUT     8000
#define RESETTIMEOUT	2000
#define REPLYTIMEOUT    1000
#define LTIME		((interval) 3000)
#define RETRANSTIME	50

#define BCRETRIAL       10
#define BCNEEDRESPONSE	8
#define FIRSTLOCATE	3

#ifndef NO_CONGESTION_CONTROL
#define RETRANS_MAX_PENDING 1	/* only allow one pending retransmission */
#define CNG_INIT_RATE	10	/* initially only send 10 Kb/timeslice */
#define CNG_MAX_RATE	120	/* only send 120 Kb/timeslice (ethernet) */
#define CNG_SLICE	100	/* data rates are per 100 msec timeslice */
#else
#define RETRANS_MAX_PENDING 4
#endif

/* Macro to determine when a member has been silent for too long.  It is
 * dependent on the member id, so that not all members at the same time send
 * a message to the sequencer.
 */
#define WAIT_SILENT(g)	(g->g_index >> g->g_soonsilent)
#define IAMSILENT(g)	((g->g_seq != g->g_me) && \
	        ((int) g->g_silent >= (int) (WAIT_SILENT(g) + g->g_maxsilent)))

/* Set release routine of a packet. */
#define SETTIMER(g, msg, timer) pkt_set_release(msg, timer, (long) group_no(g))


/*
 *	Variables declared in conf.c
 */
 
extern uint16 grp_maxgrp;	/* Maximum # of local members. */
extern f_hopcnt_t max_hops;	/* Max hops of network; needed for locating. */


/*
 *	Global variables.
 */

#ifndef GRP_DEBUG_LEVEL
#define GRP_DEBUG_LEVEL	0
#endif
int grp_debug_level;		/* determines the amount of debug output */
group_p grouptab;		/* tabel for with group structures */

/* Group number is the index in the group table */
#define group_no(g)	((g_id_t)((g) - grouptab))

static int ngroups_in_use;	/* number of groups used */

/* Members are numbered according to their index in the group's member list: */
#define mem_no(g,mp)	((mp) - (g)->g_member)
#define my_mem_no(g)	mem_no(g, (g)->g_me)
#define seq_mem_no(g)	mem_no(g, (g)->g_seq)
#define coord_mem_no(g)	mem_no(g, (g)->g_coord)

static pool_t grp_pool;		/* packet pool for group communication */
static pkt_p grp_pkt;		/* packet list for the pool */
static char *grp_pktdata;	/* data for the packets */
static port nullport;
static adr_t nulladdr;
static barrier_p freebarrier;	/* list of barriers */
static group_p *groupindex;	/* fast entry to grouptab */


/*
 * Forward declarations
 */
static void processrec _ARGS(( group_p ));
static void switchtype _ARGS(( bchdr_p ));
static void grp_arrived _ARGS(( group_p, pkt_p, adr_p, adr_p,
			       f_msgcnt_t, f_size_t, f_size_t, f_size_t ));
static void grp_notdeliver _ARGS(( group_p, pkt_p, adr_p, f_msgcnt_t,
				  f_size_t,  f_size_t, f_size_t, int ));
static void delblock _ARGS(( group_p, event ));
static void unblock _ARGS(( group_p ));


#if defined(STATISTICS) && !defined(SMALL_KERNEL)
static struct {
	int st_slocate;
	int st_shereis;
	int st_sjoinreq;
	int st_sjoin;
	int st_sleavereq;
	int st_sleave;
	int st_sbcastreq;
	int st_sbcast;
	int st_sretrans;
	int st_stimedout;
	int st_ssync;
	int st_sfullsync;
	int st_sstate;
	int st_sreformreq;
	int st_svote;
	int st_sresult;
	int st_sresultack;
	int st_salive;
	int st_salivereq;
	int st_salivereq_1;
	int st_sdead;
	int st_sretrial;
	int st_sack;
	int st_scollect;
	int st_rlocate;
	int st_rhereis;
	int st_rjoinreq;
	int st_rjoin;
	int st_rleavereq;
	int st_rleave;
	int st_rbcastreq;
	int st_rbcast;
	int st_rretrans;
	int st_rsync;
	int st_rstate;
	int st_rreformreq;
	int st_rvote;
	int st_rresult;
	int st_rresultack;
	int st_ralive;
	int st_ralivereq;
	int st_rdead;
	int st_rack;
	int st_replytimeout;
	int st_synctimeout;
} gstat;

#define STINC(type)	gstat.type++

int 
grp_statistics(begin, end)
char *begin;
char *end;
{
        char *p;
	int n;

	n =  gstat.st_sjoinreq + gstat.st_sjoin +  gstat.st_sleavereq +
	gstat.st_sleave + gstat.st_sbcastreq + gstat.st_sbcast +
	gstat.st_sretrans + gstat.st_ssync + gstat.st_sstate +
	gstat.st_sreformreq + gstat.st_svote + gstat.st_sresult +
	gstat.st_sresultack + gstat.st_salive + gstat.st_salivereq +
	gstat.st_sdead + gstat.st_slocate +  gstat.st_shereis + 
        gstat.st_sretrial + gstat.st_sack + gstat.st_salivereq_1 +
	gstat.st_sfullsync;

        p = bprintf(begin, end, "======= FLIP BCAST statistics ==========\n");
        p = bprintf(p, end, "send statistics: # mess %9ld\n", n);
        p = bprintf(p, end, "locate    %9ld ",  gstat.st_slocate);
        p = bprintf(p, end, "hereis    %9ld\n", gstat.st_shereis);
        p = bprintf(p, end, "joinreq   %9ld ",  gstat.st_sjoinreq);
        p = bprintf(p, end, "join      %9ld ",  gstat.st_sjoin);
        p = bprintf(p, end, "leavereq  %9ld ",  gstat.st_sleavereq);
        p = bprintf(p, end, "leave     %9ld\n", gstat.st_sleave);
        p = bprintf(p, end, "bcastreq  %9ld ",  gstat.st_sbcastreq);
        p = bprintf(p, end, "bcast     %9ld ",  gstat.st_sbcast);
        p = bprintf(p, end, "sync      %9ld ",  gstat.st_ssync);
        p = bprintf(p, end, "fullsync  %9ld\n", gstat.st_sfullsync);
        p = bprintf(p, end, "state     %9ld ",  gstat.st_sstate);
        p = bprintf(p, end, "alivereq1 %9ld ",  gstat.st_salivereq_1);
        p = bprintf(p, end, "alivereq  %9ld ",  gstat.st_salivereq);
        p = bprintf(p, end, "alive     %9ld\n", gstat.st_salive);
        p = bprintf(p, end, "ack       %9ld ",  gstat.st_sack);
	p = bprintf(p, end, "retrialreq%9ld ",  gstat.st_stimedout);
        p = bprintf(p, end, "retrial   %9ld ",  gstat.st_sretrial);
        p = bprintf(p, end, "retransreq%9ld\n", gstat.st_sretrans);
        p = bprintf(p, end, "reformreq %9ld ",  gstat.st_sreformreq);
        p = bprintf(p, end, "vote      %9ld ",  gstat.st_svote);
        p = bprintf(p, end, "result    %9ld ",  gstat.st_sresult);
        p = bprintf(p, end, "resultack %9ld\n", gstat.st_sresultack);
        p = bprintf(p, end, "dead      %9ld ",  gstat.st_sdead);
	p = bprintf(p, end, "collect   %9ld\n", gstat.st_scollect);

	n =  gstat.st_rjoinreq + gstat.st_rjoin +  gstat.st_rleavereq +
	gstat.st_rleave + gstat.st_rbcastreq + gstat.st_rbcast +
	gstat.st_rretrans + gstat.st_rsync + gstat.st_rstate +
	gstat.st_rreformreq + gstat.st_rvote + gstat.st_rresult +
	gstat.st_rresultack + gstat.st_ralive + gstat.st_ralivereq +
	gstat.st_rdead + gstat.st_rack;

        p = bprintf(p, end, "receive statistics:  # mess:%9ld\n", n);
        p = bprintf(p, end, "joinreq   %9ld ",  gstat.st_rjoinreq);
        p = bprintf(p, end, "join      %9ld ",  gstat.st_rjoin);
        p = bprintf(p, end, "leavereq  %9ld ",  gstat.st_rleavereq);
        p = bprintf(p, end, "leave     %9ld\n", gstat.st_rleave);
        p = bprintf(p, end, "bcastreq  %9ld ",  gstat.st_rbcastreq);
        p = bprintf(p, end, "bcast     %9ld ",  gstat.st_rbcast);
        p = bprintf(p, end, "retrans   %9ld ",  gstat.st_rretrans);
        p = bprintf(p, end, "sync      %9ld\n", gstat.st_rsync);
        p = bprintf(p, end, "state     %9ld ",  gstat.st_rstate);
        p = bprintf(p, end, "alivereq  %9ld ",  gstat.st_ralivereq);
        p = bprintf(p, end, "alive     %9ld ",  gstat.st_ralive);
        p = bprintf(p, end, "ack       %9ld\n", gstat.st_rack);
        p = bprintf(p, end, "reformreq %9ld ",  gstat.st_rreformreq);
        p = bprintf(p, end, "vote      %9ld ",  gstat.st_rvote);
        p = bprintf(p, end, "result    %9ld ",  gstat.st_rresult);
        p = bprintf(p, end, "resultack %9ld\n", gstat.st_rresultack);
        p = bprintf(p, end, "dead      %9ld\n", gstat.st_rdead);
        p = bprintf(p, end, "general statistics:\n");
        p = bprintf(p, end, "replytimeout  %9ld ", gstat.st_replytimeout);
        p = bprintf(p, end, "synctimeout  %9ld\n", gstat.st_synctimeout);
        return p - begin;
}
#else	/* !STATISTICS || SMALL_KERNEL */
#define STINC(st_type)
#endif	

/*
 * 	Set timer functions.
 */

static void setsynctimer(g)
    group_p g;
{
    g->g_synctimer = g->g_stimeout;
}

static void setalivetimer(g)
    group_p g;
{
/* Set alive timeout. */

    if(!(g->g_flags & FL_RESET)) g->g_alivetimer = g->g_atimeout;
}

/*
 *	Packet timers used with SETTIMER.
 */

static void settimer(gno)
    g_id_t gno;
{
/* When a packet is discarded by a lower layer, the reply timer is set. */
    group_p g;

    assert(0 <= gno && gno < (int) grp_maxgrp);
    g = &grouptab[gno];

    if(g->g_flags & (FL_JOINING | FL_SENDING)) {
#ifndef NO_CONGESTION_CONTROL
	if (g->g_cng_retrans > 0) {
	    /* use the retrans timer as computed by the congestion avoidance
	     * algorithm.
	     */
	    g->g_replytimer = g->g_cng_retrans;
	} else {
	    g->g_replytimer = g->g_rtimeout;
	}
#else
	g->g_replytimer = g->g_rtimeout;
#endif
    }
}


static void setresettimer(gno)
    g_id_t gno;
{
/* Set reset timeout. */
    group_p g;

    assert(0 <= gno && gno < (int) grp_maxgrp);
    g = &grouptab[gno];

    g->g_resettimer = g->g_resettimeout;
}

static void setresettimer_multi(gno)
    g_id_t gno;
{
    /* Identical to setresettimer(), but has different semantics in case the
     * packet cannot be delivered.  See grp_notdeliver().
     */
    setresettimer(gno);
}


/*
 *	General group structure manipulation functions.
 */

static int group_exist(pid, p)
    int pid;
    port *p;
{
/* Does member pid exist? */

    group_p g;
    
    for(g = grouptab; g < grouptab + grp_maxgrp; g++) 
	if(g->g_pid == pid && PORTCMP(p, &g->g_port) && g->g_used &&
	   !((g->g_flags & FL_DESTROY) && (g->g_flags & FL_NOUSER))) {
	    return(1);
	}
    return 0;
}


static group_p getgroup(p)
    port *p;
{
/* Get a free group entry */

    group_p g;

    for(g = grouptab; g < grouptab + grp_maxgrp; g++) 
	if(!g->g_used) {
	    (void) memset((_VOIDSTAR) g, 0, sizeof(group_t));
	    g->g_used = 1;
	    g->g_port = *p;
	    g->g_bcastep = -1;
	    g->g_ep = -1;
	    g->g_ep_all = -1;
	    g->g_ifno = flip_init((long ) g, grp_arrived, grp_notdeliver);
    	    g->g_pid = getpid(curthread);
	    g->g_flags = 0;
	    g->g_member = 0;
	    ngroups_in_use++;
	    return(g);
	}
    return 0;
}


static void freegroup(g)
    group_p g;
{
    if(flip_end(g->g_ifno) < 0) 
	panic("freegroup: flip_end failed");
    g->g_replytimer = -1;
    g->g_synctimer = -1;
    g->g_resettimer = -1;
    g->g_alivetimer = -1;
    g->g_used = 0;
    g->g_pid = -1;
    g->g_flags = 0;
    g->g_port = nullport;
    g->g_memnbuf = 0;
    g->g_hstnbuf = 0;
    g->g_bufnbuf = 0;
    g->g_nbuf = 0;
    g->g_fbarrier = 0;
    ngroups_in_use--;
}

static void grp_freebufs(g)
    group_p g;
{
    member_p m;

    if (g->g_member != 0) {
	for (m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	    if (m->m_state != M_UNUSED) {
		grmem_del(g, m);
	    }
	}
	relblk((vir_bytes) g->g_member);
	g->g_member = 0;
    }
    if (g->g_history != 0) {
	relblk((vir_bytes) g->g_history);
	g->g_history = 0;
    }
    if (g->g_mem_descr.mal_valid) {
	mal_delete(&g->g_mem_descr);
    }
}

static int initgroup(g)
    group_p g;
{
/* Init a group. First malloc data. If this fails, the initialization fails. */

    int i;
    member_p m;
    char *buf;
    
    if (g->g_nloghist <= 2) return(0);
    if (g->g_resilience == 0 && g->g_index != g->g_seqid) {
	/* We have tried to give the regular members less history and message
	 * buffers in the non-resilient case in order to reduce the memory
	 * requirements.  Unfortunately, this seems to break a few invariants
	 * or assumptions in the code that still have to be figured out.
	 * For the moment just reduce the number of buffers.
	 */
	g->g_nhist = 1 << g->g_nloghist;
	/* Note that we're allocating only slightly more buffer space
	 * than the full history size.  The assumption is that not all
	 * members will be broadcasting new data (that has to be buffered
	 * directly in case of BB) at the same time.  If that's really
	 * happening, it will still run, although it may require many
	 * retransmissions.
	 * To avoid this, we need to make the buffering policy more flexible.
	 * Not every message really requires a buffer of the maximum size;
	 * in fact, typically most messages will be rather small.
	 */
	g->g_nbuf = g->g_nhist + g->g_maxgroup / 2 + 2; 
    } else {
	g->g_nhist = 1 << g->g_nloghist;
	g->g_nbuf = g->g_nhist + 2 * g->g_maxgroup + 2; 
    }
    g->g_nhstfull = g->g_nhist - g->g_maxgroup - 2;
    if (g->g_nhstfull <= 0) return(0);
    g->g_hstmask = ~(~0 << g->g_nloghist);	/* modulo operator */
    /* g_logsizemess determines the max message size used by the user.
     * However, an additional Amoeba header is sent along with each message,
     * so add that to the size.
     */
    g->g_maxsizemess = (1 << g->g_logsizemess) + sizeof(header);
    /* Since the group protocol also sends its own control messages (e.g.,
     * for JOIN) determine how large the buffers have to be for them.
     * It depends on the maximum group size.
     */
    g->g_maxctrlsize = grp_max_marshall_size(g);
    g->g_bufnbuf = g->g_nbuf;
    g->g_hstnbuf = 0;
    g->g_memnbuf = 0;

    g->g_stimeout = SYNCTIMEOUT;
    g->g_rtimeout = REPLYTIMEOUT;
    g->g_atimeout = ALIVETIMEOUT;
    g->g_ltimeout = LTIME;
    g->g_resettimeout = RESETTIMEOUT;
    g->g_maxretrial = BCRETRIAL;

    /* Buffers not yet initialized: */
    g->g_history = 0;
    g->g_member = 0;
    g->g_mem_descr.mal_valid = 0;

    GDEBUG(LB, printf("hist=getblk(%d)\n", sizeof(hist_t) * g->g_nhist));
    g->g_history = (hist_p) getblk((vir_bytes)(sizeof(hist_t) * g->g_nhist));
    if (g->g_history == 0) {
	GDEBUG(LM, printf("could not allocate history buffers\n"));
	grp_freebufs(g);
	return(0);
    }
    for (i = 0; i < (int) g->g_nhist; i++) {
	g->g_history[i].h_size = 0;
	g->g_history[i].h_data = 0;
	g->g_history[i].h_bc.b_seqno = 0;
    }

    GDEBUG(LB, printf("memb=getblk(%d)\n", sizeof(member_t) * g->g_maxgroup));
    g->g_member = (member_p)
			getblk((vir_bytes)(sizeof(member_t) * g->g_maxgroup));
    if (g->g_member == 0) {
	GDEBUG(LM, printf("could not allocate member buffers\n"));
	grp_freebufs(g);
	return(0);
    }
    for (m = g->g_member; m < &g->g_member[g->g_maxgroup]; m++) {
	grmem_init(g, m);
    }

    /* Now initialize the message buffer allocation module.
     * Note that grp_initbuf itself doesn't allocate any memory yet.
     */
    grp_initbuf(g);

    /* Now try to preallocate a single buffer.  In the current implementation,
     * this will cause a large segment to be allocated.  If that doesn't work,
     * there's no chance we can participate succesfully in a group (resilient
     * or not) so in that case we fail right away.
     */
    GRP_GETBUF(g, buf, g->g_maxsizemess);
    if (buf == 0) {
	GDEBUG(LM, printf("could not allocate message buffers\n"));
	grp_freebufs(g);
	return(0);
    }
    GRP_FREEBUF(g, buf);

#ifndef NO_CONGESTION_CONTROL
    g->g_cng_timer = g->g_cng_timeout = 5000;
    g->g_cng_cur_rate = CNG_INIT_RATE;
    g->g_cng_thresh_rate = CNG_MAX_RATE / 2;
    g->g_cng_sent = 0;
    g->g_cng_tot_sent = 0;
    g->g_cng_start_send = 0;
    g->g_cng_start_slice = 0;
#endif
    g->g_incarnation  = 0;
    g->g_total = 0;
    g->g_total_noseq = 0;
    g->g_coord = 0;
    g->g_last_sender = 0;
    g->g_memrank = 0;
    g->g_nextseqno = 1;		/* don't use seqno == 0 */
    g->g_retrans_seq = 0;
    g->g_retrans_pending = 0;
    g->g_retrans_time = 0;
    g->g_minhistory = 1;
    g->g_nexthistory = 1;
    g->g_replytimer = -1;
    g->g_synctimer = -1;
    g->g_silent = 0;
    /* Depending on the history size and group size, we figure out when
     * a member should send an acknowledgement, while trying to avoid
     * sending all acknowledgements at the same time.
     */
    if ((int) g->g_nhist >= (int) (g->g_maxgroup << 2)) {
	/* plenty of history buffers; notify sequencer if we received
	 * more than half of the history size.
	 */
	g->g_maxsilent = g->g_nhist >> 1;
	g->g_soonsilent = 0;
    } else {
	/* notify sequencer earlier */
	g->g_maxsilent = g->g_nhist >> 2;
	if ((int) g->g_nhist >= (int) (g->g_maxgroup << 1)) {
	    g->g_soonsilent = 1;
	} else {
	    g->g_soonsilent = 2;
	}
    }
    g->g_fbarrier = 0;
    g->g_lbarrier = &g->g_fbarrier;
    g->g_large = BCLARGE;
    return(1);
}


static g_index_t rank(g, cpu)
    group_p g;
    g_index_t cpu;
{
/* Compute rank for member. The rank is the order in group. Sequencer has
 * rank 0. The next member has rank 1, etc.
 */

    g_index_t i;
    g_index_t rank = 0;

    if(cpu == g->g_seqid) return(0);
    for(i=0; i < g->g_maxgroup; i++) {
	if(IS_MEMBER(g->g_member[i].m_state) && g->g_seqid != i)
	    rank++;
	if(i == cpu) return(rank);
    }
    panic("rank: this should not be possible");
    /*NOTREACHED*/
}

static void
grp_unregister_bcast_ep(g)
    group_p g;
{
    if (g->g_bcastep >= 0) {
	if (flip_unregister(g->g_ifno, g->g_bcastep) < 0) {
	    /* shouldn't happen */
	    printf("grp_unregister_bcast_ep failed\n");
        }
	g->g_bcastep = -1;
    }
}

static void
grp_unregister_my_ep(g)
    group_p g;
{
    if (g->g_me->m_ep >= 0) {
	if (flip_unregister(g->g_ifno, g->g_me->m_ep) < 0) {
	    /* shouldn't happen */
	    printf("grp_unregister_my_ep failed\n");
	}
	g->g_me->m_ep = -1;
    }
}

static void
grp_unregister_ep_all(g)
    group_p g;
{
    if (g->g_ep_all >= 0) {
	if (flip_unregister(g->g_ifno, g->g_ep_all) < 0) {
	    /* shouldn't happen */
	    printf("grp_unregister_ep_all failed\n");
	}
	g->g_ep_all = -1;
    }
}

static void
grp_unregister_ep(g)
    group_p g;
{
    if (g->g_ep >= 0) {
	if (flip_unregister(g->g_ifno, g->g_ep) < 0) {
	    /* shouldn't happen */
	    printf("grp_unregister_ep failed\n");
	}
	g->g_ep = -1;
    }
}

static void
deletegroup(g)
    group_p g;
{
/* Delete a group entry. Some addresses may have been unregistered earlier. */

    barrier_p bp, next;

    assert(g->g_used);
    GDEBUG(LM, printf("deletegroup %d: unregister (me = %d, seq = %d)\n",
			group_no(g), my_mem_no(g), seq_mem_no(g)));
    GDEBUG(LB, grp_dump((char *) 0, (char *) 0));
    if (g->g_me == g->g_seq) {
	grp_unregister_bcast_ep(g);
    }
    grp_unregister_my_ep(g);
    grp_unregister_ep_all(g);
    grp_unregister_ep(g);

    grp_freebufs(g);

    /* Remove the barriers */
    for(bp = g->g_fbarrier; bp != 0; bp = next) {
	next = bp->b_next;
	wakeup((event) &bp->b_barrier);
	bp->b_next = freebarrier;
	freebarrier = bp;
    }
    g->g_fbarrier = 0;
    freegroup(g);
}


/*
 *	Functions to build and send a message.
 */


static int fillmsg(g, msg, hdr, type, seqno, messid, cpu, data, n)
    register group_p g;
    register pkt_p *msg;
    register header_p hdr;
    uint8 type;
    g_seqcnt_t seqno;
    g_msgcnt_t messid;
    g_index_t cpu;
    char *data;
    f_size_t n;
{
/* Get a packet and build a complete message. */
    register pkt_p pmsg;

    PKT_GET(*(msg), &grp_pool);
    if((pmsg = *msg) == 0) {
	printf("WARNING: fillmsg out of packets (");
	bc_printtype((int) type);
	printf(" seq %ld mid %ld cpu %d)\n", seqno, messid, cpu);
	return(0);
    } else {
	register bchdr_p phdr;

	proto_init(pmsg);
	if(hdr != 0) proto_dir_append(pmsg, (char *) hdr, sizeof(header));
	if(n > 0) {	
	    proto_set_virtual(pmsg, 0, 0, (long) data, n);
	}

	PROTO_ADD_HEADER(phdr, pmsg, bchdr_t);
	phdr->b_type = type;
	phdr->b_flags = 0;
	phdr->b_reserved = 0;
	bc_setendian(phdr);
	phdr->b_seqno = seqno;
	phdr->b_messid = messid;
	phdr->b_cpu = cpu;
	phdr->b_incarnation = g->g_incarnation;
	phdr->b_expect = (g->g_member == 0) ? 0 : g->g_member[cpu].m_expect;
	GDEBUG(LF, bc_print(phdr); printf("\n"));
	proto_fix_header(pmsg);
	pmsg->p_admin.pa_priority = bc_highpriority(type);
	return(1);
    }
}


static int setmsg(g, msg, hdr, bc, data, n)
    register group_p g;
    register pkt_p *msg;
    register header_p hdr;
    register bchdr_p bc;
    char *data;
    f_size_t n;
{
/* Same as previous, but the arguments are stored in bc. */
    register pkt_p pmsg;
    
    PKT_GET(*(msg), &grp_pool);
    if((pmsg = *msg) == 0) {
	printf("WARNING: setmsg: out of packets (");
	bc_printtype((int) bc->b_type);
	printf(" seq %ld mid %ld cpu %d)\n", bc->b_seqno,
	       bc->b_messid, bc->b_cpu);
	return(0);
    } else {
	register bchdr_p phdr;

	proto_init(pmsg);
	if(hdr != 0) proto_dir_append(pmsg, (char *) hdr, sizeof(header));
	if(n > 0) {
	    proto_set_virtual(pmsg, 0, 0, (long) data, (long) n);
	} 

	PROTO_ADD_HEADER(phdr, pmsg, bchdr_t);
	*phdr = *bc;
	phdr->b_flags = 0;
	phdr->b_reserved = 0;
	bc_setendian(phdr);
	phdr->b_incarnation = g->g_incarnation;
	phdr->b_expect =
	    (g->g_member == 0) ? 0 : g->g_member[bc->b_cpu].m_expect;
	GDEBUG(LF, bc_print(phdr); printf("\n"));
	proto_fix_header(pmsg);
	pmsg->p_admin.pa_priority = bc_highpriority(phdr->b_type);
	return(1);
    }
}


/* 
 *	Set of help functions that are called during recovery.
 */

static void switchstate(g)
    group_p g;
{
/* Some member died. Stop with everything and wait until the application
 * initiates the recovery phase. Unregister group addresses to make sure
 * that we do not accept messages that are directed to the group.
 */

    GDEBUG(LM, printf("switchstate: reset group %d\n", group_no(g)));
    GDEBUG(LM, printf("(me = %d, seq = %d)\n", my_mem_no(g), seq_mem_no(g)));

    if(g->g_flags & FL_NOUSER) {
        deletegroup(g);
        return;
    }

    if(g->g_flags & FL_RESET) return;
    g->g_flags |= FL_RESET;
    g->g_synctimer = -1;
    g->g_alivetimer = -1;
    if (g->g_me == g->g_seq) {
	grp_unregister_bcast_ep(g);
    } else {
	grp_unregister_ep(g);
    }
    grp_unregister_ep_all(g);
    if(g->g_flags & FL_RECEIVING) {
	wakeup((event) &g->g_receiver);
    }
    if(g->g_flags & FL_SENDING && g->g_flags & FL_JOINING) {
	wakeup((event) &g->g_sender);
    }
    if(g->g_resilience > 0) {
	/* For a resilience > 0, messages are buffered in the free part of
	 * the history, waiting on the final ok of the sequencer. These are
	 * the messages for which an ack has been sent and for which the
	 * sequencer did not announce that they have been received by
         * resilience kernels. The sequencer could, however, have delivered
	 * message and crashed, so we have to accept these messages as an 
	 * official accepted msg. (Before grp_reset returns it is guaranteed
	 * that all alive members will have it (the coordinator updates all the
	 * histories), so there is no danger that only 1 member will have it and
	 * deliver it to the application.) The following code accepts messages
	 * buffered in the free part by increasing nextseqno until there is a
	 * gap. The message after the gap can be discarded, because they cannot
	 * have been accepted.
	 * If the sequencer itself is still alive, it accepts the message
	 * stored in the third part of the history (from g_nextseqno until
	 * g_nexthistory). As the histories are replicated before the recovery
	 * is considered successful, this is allowed.
	 */
	g_seqcnt_t seq;
	hist_p hist;

	for (seq = g->g_nextseqno; 1 ; seq++) {
	    /* TODO: a rough upperbound, so that we detect an endless loop.
	     * Useless: assert(seq <= seq + g->g_nextseqno + g->g_total + 1);
	     */
	    hist = &g->g_history[HST_MOD(g, seq)];
	    if (hist->h_bc.b_seqno < seq)
		break;
	    printf("msg %d is officially accepted\n", hist->h_bc.b_seqno);
	    compare(hist->h_bc.b_seqno, ==, seq);
	    hist->h_accept = 1;
	    switchtype(&hist->h_bc);
	}
	compare(seq - g->g_me->m_expect, <, g->g_nhist);
	if(g->g_seq == g->g_me) {
	    compare(seq, ==, g->g_nexthistory);
	    /* processreq(g); */
	    g->g_nextseqno = seq;
	} else processrec(g);
	compare(g->g_nextseqno, ==, seq);
    }
    hst_buffree(g); 		/* remove additional buffered messages. */
}


/*
 * To avoid forgetting to do a pkt_discard, or failures to pass unnoticed,
 * use do_flip_[uni/multi]cast() instead of flip_[uni/multi]cast().
 */
static char *unicast_errstring = "%s: flip_unicast failed (%d)\n";

#define do_flip_unicast(ifno, pkt, flags, dst, ep, length, ltime, mesg) { \
int ferr = flip_unicast(ifno, pkt, flags, dst, ep, length, ltime);	\
									\
  if (ferr < 0) {							\
	GDEBUG(LM, printf(unicast_errstring, mesg, ferr));		\
	pkt_discard(pkt);						\
  }									\
}

static char *multicast_errstring = "%s: flip_multicast failed (%d)\n";

#define do_flip_multicast(ifno, pkt, flags, dst, ep, length, n, ltime, mesg) {\
int ferr = flip_multicast(ifno, pkt, flags, dst, ep, length, n, ltime);	\
									\
  if (ferr < 0) {							\
	GDEBUG(LM, printf(multicast_errstring, mesg, ferr));		\
	pkt_discard(pkt);						\
  }									\
}


static void sendreform(g)
    group_p g;
{
/* Send the members of the group a reform message to try to build a new group
 * from the old one.
 */

    member_p m;
    pkt_p msg;
    
    assert(g->g_flags & FL_COORDINATOR);
    for (m = g->g_member;
	 (m < g->g_member + g->g_maxgroup) && (g->g_flags & FL_COORDINATOR);
	 m++)
    {
	if(!IS_MEMBER(m->m_state)) continue;
	if(m->m_vote != SILENT) continue;
	GDEBUG(LM, printf("sendreform(%d): send %d a reformreq\n",
			  group_no(g), mem_no(g, m)));
	if(fillmsg(g, &msg, (header_p) 0, BC_REFORMREQ, g->g_nextseqno, 0L,
		  g->g_index, (char *) 0, 0L)) {
	    STINC(st_sreformreq);
	    SETTIMER(g, msg, setresettimer_multi);
	    do_flip_unicast(g->g_ifno, msg, 0, (adr_p) &m->m_addr,
			    g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
			    g->g_ltimeout, "sendreform");
	}
    }
}


static void ask_for_retrans(g, cpu, last)
    group_p g;
    g_index_t cpu;
    g_seqcnt_t last;
{
    /* Ask cpu for retransmission of the missing messages with seqno
     * in range [g->g_nextseqno .. last].  Don't ask for more than
     * RETRANS_MAX_PENDING retransmissions. Also avoid asking the same
     * messages several times.
     */
    g_seqcnt_t seq;
    pkt_p msg;
    unsigned long now;

    if (g->g_nextseqno <= g->g_retrans_seq && g->g_retrans_pending > 0) {
	GDEBUG(LM, printf("retrans %d pending\n", g->g_nextseqno));
	g->g_retrans_pending--;
	return;
    }
    /* Likewise, if we've asked for a retransmission very recently,
     * don't ask it again.  The reponse (or maybe the request itself)
     * is probably still somewhere in the pipeline.
     */
    now = getmilli();
    if (now > g->g_retrans_time && (now - g->g_retrans_time) < RETRANSTIME) {
	GDEBUG(LM, printf("last retrans req was %d ms ago\n",
			  now - g->g_retrans_time));
	return;
    }

    g->g_retrans_pending = 0;
    for (seq = g->g_nextseqno;
 	 seq <= last && g->g_retrans_pending < RETRANS_MAX_PENDING;
 	 seq++)
    {
	hist_p hist;

	hist = &g->g_history[HST_MOD(g, seq)];
	if (seq == g->g_nextseqno || hist->h_bc.b_seqno != seq) {
	    if (fillmsg(g, &msg, (header_p) 0, BC_RETRANS, seq, 0L, 
	    		g->g_index, (char *) 0, 0L))
	    {
		STINC(st_sretrans);
		GDEBUG(LM, printf("ask retrans %d\n", seq));
		/* Temporarily set g_retrans_seq one higher, to avoid
		 * another ask_for_retrans call when we get an immediate
		 * response because the other member is on the same cpu.
		 */
		g->g_retrans_seq = seq + 1;
		g->g_retrans_pending++;
		do_flip_unicast(g->g_ifno, msg, 0, &g->g_member[cpu].m_addr,
				g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
				g->g_ltimeout, "ask_for_retrans");
		g->g_retrans_seq = seq;
	    }
	}
    }

    if (g->g_retrans_pending > 0) {
	g->g_retrans_time = now;
    }
}


static int collectmsg(g, cpu, s)
    group_p g;
    g_index_t cpu;
    g_seqcnt_t s;
{
    /* The coordinator does not have the message from g_nextseqno until s
     * in its history buffer. It has to collect them from cpu.
     */
    pkt_p msg;
    g_seqcnt_t prev;

    GDEBUG(LM, printf("%d: collect at %d missing messages from %d until %d\n",
		      group_no(g), cpu, g->g_nextseqno, s));

    g->g_flags |= FL_COLLECTING;
    g->g_member[cpu].m_retrial = g->g_maxretrial;
    do {
	prev = g->g_nextseqno;
	if(fillmsg(g, &msg, (header_p) 0, BC_RETRANS, g->g_nextseqno, 0L, 
		   g->g_index, (char *) 0, 0L)) {
	    STINC(st_sretrans);
	    STINC(st_scollect);
	    SETTIMER(g, msg, setresettimer);
	    do_flip_unicast(g->g_ifno, msg, 0, &g->g_member[cpu].m_addr,
			    g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
			    g->g_ltimeout, "collectmsg");
	}
	if(g->g_flags & FL_COLLECTING) {
	    if (await_reason((event) &g->g_reset, 0L, "collectmsg") < 0) {
		GDEBUG(LM, printf("collectmsg: await failed\n"));
		g->g_flags &= ~FL_COLLECTING;
		return(0);
	    }
	}
	if(g->g_flags & FL_COLLECTING && g->g_nextseqno == prev) {
	    GDEBUG(LM, printf("collectmsg(%d): timeout\n", group_no(g)));
	    if(g->g_member[cpu].m_retrial-- <= 0) {
		GDEBUG(LM, printf("collectmsg(%d) failed\n", group_no(g)));
		g->g_flags &= ~FL_COLLECTING;
		return(0);
	    }
	} else g->g_member[cpu].m_retrial = g->g_maxretrial;
    } while(s > g->g_nextseqno);
    g->g_flags &= ~FL_COLLECTING;
    GDEBUG(LM, printf("collectmsg(%d) succeeded\n", group_no(g)));
    return(1);
}


static int update_members(g)
    group_p g;
{
/* Coordinator brings all the members are up to date. When grp_reset returns,
 * its history is replicated fully on all remaining members.
 */

    member_p m;
    pkt_p msg;
    g_index_t mem;

    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(!IS_MEMBER(m->m_state)) continue;
	if(m->m_vote < 0) continue; 	/* skip; this member is dead */
	mem = mem_no(g, m);
	m->m_retrial = g->g_maxretrial;
	while(m->m_vote != g->g_nextseqno) { /* update needed? */
	    GDEBUG(LM, printf("update_members: send %d bc_sync %d\n",
			      mem, g->g_nextseqno));
	    m->m_vote = 0;
	    if(fillmsg(g, &msg, (header_p) 0, BC_SYNC, g->g_nextseqno,
			(g_msgcnt_t) 0, g->g_index, (char *) &mem,
			(f_size_t) sizeof(g_index_t))) {
		SETTIMER(g, msg, setresettimer);
		STINC(st_sresult);
		do_flip_unicast(g->g_ifno, msg, 0, &m->m_addr, g->g_me->m_ep,
				(f_size_t) (sizeof(bchdr_t)+sizeof(g_index_t)),
				g->g_ltimeout, "update_members");
	    }
	    if(!m->m_vote) {
		if (await_reason((event)&g->g_reset, 0L, "update_mem") < 0) {
		    GDEBUG(LM, printf("update_members: await failed\n"));
		    return(0);
		}
	    }
	    if(m->m_retrial-- <= 0) {
		/* TODO: if there is a group of 5, and one them crashes,
		 * the fact that another one crashes during recovery doesn't
		 * mean that the whole recovery process has to fail.
		 */
		GDEBUG(LM, printf("update_members: could not help %d\n", mem));
		return(0);
	    }
	}
    }
    return(1);
}


static int restartgroup(g)
    group_p g;
{
/* The coordinator enters the second phase of recovery. */

    member_p m;
    
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	m->m_retrial = g->g_maxretrial;
	m->m_replied = 0;
    }
    g->g_me->m_replied = 1;
    g->g_flags |= FL_RESULT;
}


static void sendresult(g, success, buf, size)
    group_p g;
    int success;
    char *buf;
    f_size_t size;
{
/* The new group is built by the coordinator. Inform the new members. */

    g_seqcnt_t seqcnt;
    g_msgcnt_t messid = g->g_me->m_messid + 1;
    pkt_p msg;
    member_p m;
    
    assert(g->g_flags & FL_COORDINATOR);
    seqcnt = success ? g->g_nextseqno : 0;
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(!IS_MEMBER(m->m_state)) continue;
	if(m->m_replied) continue;
	if(fillmsg(g, &msg, (header_p) 0, BC_RESULT, seqcnt, messid,
		   g->g_index, buf, size)) {
	    SETTIMER(g, msg, setresettimer_multi);
	    STINC(st_sresult);
	    do_flip_unicast(g->g_ifno, msg, 0, &m->m_addr, g->g_me->m_ep, 
			    (f_size_t) (sizeof(bchdr_t) + size),
			    g->g_ltimeout, "sendresult");
	}
    }
}


static int crash(g)
    group_p g;
{
/* Determine which members have crashed. */

    int alive = 0, dead = 0;
    member_p m;
    
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(!IS_MEMBER(m->m_state)) continue;
	if(m->m_replied) 
	    alive++;
	else if(m->m_retrial <= 0 && !m->m_replied) 
	    dead++;
    }
    if(dead + alive >= (int) g->g_total) return(dead > 0);
    else return 0;
}


static int votefinished(g)
    group_p g;
{
/* Determine if the voting phase is finished. */

    member_p m;
    
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(!IS_MEMBER(m->m_state)) continue;
	if(!m->m_replied && m->m_retrial > 0)
	    return 0;
    }
    return 1;
}


static int repliedfinished(g)
    group_p g;
{
/* Determine if the building phase is finished. */

    member_p m;
    
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(!IS_MEMBER(m->m_state)) continue;
	if(!m->m_replied) return 0;
    }
    return 1;
}


static g_index_t numbervotes(g, voter)
    group_p g;
    g_index_t *voter;
{
/* Count the votes in favor. Voter points to a member that has the highest
 * sequence number.
*/

    int s = -1;
    g_index_t n = 0;
    member_p m, v;
    
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(!IS_MEMBER(m->m_state)) continue;
	if(m->m_vote >= 0) n++;
	if(m->m_vote > s) {
	    s = m->m_vote;
	    v = m;
	}
    }
    if(voter != 0 && n > 0) *voter = (g_index_t) mem_no(g, v);
    return n;
}


static void buildgroup(g)
    group_p g;
{
/* Build the new group. */

    member_p m;
    
    g->g_seq = g->g_coord;
    g->g_seqid = coord_mem_no(g);
    g->g_total = numbervotes(g, (g_index_t *) 0);
    g->g_incarnation++;
    g->g_memrank = 0;
    adr_random(&g->g_prvaddr);
    flip_oneway(&g->g_prvaddr, &g->g_addr);
    adr_random(&g->g_prvaddr_all);
    flip_oneway(&g->g_prvaddr_all, &g->g_addr_all);
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(!IS_MEMBER(m->m_state)) continue;
	if(m->m_vote < 0) m->m_state = M_UNUSED;
    }
    if (IS_MEMBER(g->g_member[g->g_seqid].m_state)) {
	g->g_total_noseq = g->g_total - 1;
    } else {
	/* no member running on the sequencer */
	g->g_total_noseq = g->g_total;
    }
    GDEBUG(LM, printf("buildgroup: g_total = %d, g_total_noseq = %d\n",
		      g->g_total, g->g_total_noseq));
}



/*
 * 	General help functions.
 */

static void sendstate(g)
    group_p g;
{
/* Tell the sequencer which message we have received. */

    pkt_p msg;
    
    if(fillmsg(g, &msg, (header_p) 0, BC_STATE, g->g_nextseqno, 0L, g->g_index,
	       (char *) 0, 0L)) {
	STINC(st_sstate);
	do_flip_unicast(g->g_ifno, msg, 0, &g->g_seq->m_addr, g->g_me->m_ep, 
			(f_size_t)sizeof(bchdr_t), g->g_ltimeout, "sendstate");
    }
}

#define GRP_MAX_MEMBERS	512

static void synchronize(g, timeout)
    group_p g;
    int     timeout;
{
/* This routine tries to synchronize the sequencer and all members in the
 * group. It sends to all members a message telling where the sequencer is
 * and asks all members that are behind to respond to this message.
 * If all members are dead and the sequencer does not run a user process,
 * the group is discarded.
 * After a sync timeout, the sequencer has to check all members of whom it
 * does not know if they have received all messages. Otherwise, it only checks 
 * the members that cause the history to be full.
 */

    /* the next should be dynamically allocated: */
    static g_index_t grpmemlist[GRP_MAX_MEMBERS];
    pkt_p msg;
    g_index_t id;
    int f = 0;
    register member_p m;
    char *p, *end;
    f_size_t nbytes;
    int nmembers;
    
    if ((g->g_flags & FL_NOUSER) && (g->g_flags & FL_RESET)) {
	deletegroup(g);
	return;
    }

    if (!timeout) {
	if (g->g_synctimer > 100) {
	    /* do a full synchronisation 100 msec from now */
	    g->g_synctimer = 100;
	} else {
	    /* Synchronisation is going to happen very soon (possibly since
	     * we were called only a moment ago) so no need to do it now.
	     */
	    GDEBUG(LM, printf("synch pending\n"));
	    return;
	}
    }

    if(g->g_replytimer > 0) {
	/* keep the sequencer from retransmissions */
    	settimer(group_no(g));
    }

    if(g->g_total == 1) {
	(void) hst_free(g);
    }

    GDEBUG(LM, printf("synchronize: g_nextseq = %d\n", g->g_nextseqno));

    p = (char *) grpmemlist;
    end = (char *) &grpmemlist[GRP_MAX_MEMBERS];
    nmembers = 0;
    for(m=g->g_member, id = 0; m < g->g_member + g->g_maxgroup; m++, id++) {
	if(m->m_state == M_UNUSED) continue;
	if(m == g->g_seq) continue;
	if((timeout && m->m_expect != g->g_nextseqno) ||
	   (!timeout && m->m_expect == g->g_minhistory)) {
	    p = grp_marshall_uint16(p, end, &id);
	    nmembers++;
	    if (timeout) {
		if (m->m_retrial > FIRSTLOCATE) {
		    m->m_retrial--;
		}
		if (m->m_retrial <= FIRSTLOCATE) {
		    /* i is not reachable, try to locate i */
		    f |= FLIP_INVAL;
		}
	    } else {
		GDEBUG(LM, printf("sync %d: m_expect = %d\n",
				  id, m->m_expect));
	    }
	}
    }
    if (p == NULL) {
	GDEBUG(LR, printf("sync: marshalling error\n"));
	return;
    }
    nbytes = p - (char *) grpmemlist;
    if (nbytes > 0) {
	/* Send the sync message. Memlist contains the ids of members
	 * that have to respond.
	 */
	GDEBUG(LM, printf("synch %d members\n", nmembers));
	if (fillmsg(g, &msg, (header_p) 0, BC_SYNC, g->g_nextseqno, 0L, 
		    g->g_seqid, (char *) grpmemlist, nbytes)) {
	    if (timeout) {
		STINC(st_ssync);
	    } else {
		STINC(st_sfullsync);
	    }
	    do_flip_multicast(g->g_ifno, msg, f, &g->g_addr, g->g_me->m_ep, 
			      (f_size_t) (sizeof(bchdr_t) + nbytes),
			      MAX(g->g_total_noseq, 1), g->g_ltimeout,
			      "synchronize");
	}
    }
}


static void checkliveness(g)
    group_p g;
{
/* Check if the sequencer is alive or if the members are alive. If a member
 * or the sequencer has sent a message since the last time we have checked,
 * do not check.
 */

    member_p m;
    g_index_t d = 0;
    int need_response = 0;
    int want_response = 0;
    int f = 0;
    pkt_p msg;
    
    for(m = g->g_member; m != 0 && m < g->g_member + g->g_maxgroup; m++) {
	if(m->m_state == M_UNUSED) continue;
	if(m == g->g_me) continue;
	if(!m->m_replied) {
	    /* To avoid a quadratic number of alive messages, normal
	     * members only make sure the sequencer is alive.
	     * The sequencer will check the liveness of all other members.
	     */
	    if (g->g_me == g->g_seq || m == g->g_seq)
	    {
		m->m_retrial--;
		if (m->m_retrial < BCNEEDRESPONSE) {
		    /* It is getting critical: do a separate unicast for
		     * this member.
		     */
		    need_response++;
		}
		if (m->m_retrial < g->g_maxretrial) {
		    want_response++;
		}
		if(m->m_retrial <= FIRSTLOCATE) f |= FLIP_INVAL;
		if (m->m_retrial <= 0) {
		    d++;
		    GDEBUG(LM, printf("checkliveness: %d didn't reply\n",
				      mem_no(g, m)));
		    if(m->m_state == M_LEAVING || m->m_state == M_JOINING) {
			grmem_del(g, m);
		    } else {
			d++;
			switchstate(g);
			return;	/* stop checking; group address in not valid */
		    }
		}
	    }
	} else {
	    m->m_retrial = g->g_maxretrial;
	}
	m->m_replied = 0;
    }

    if((g->g_flags & FL_NOUSER) && g->g_total <= d) {
	deletegroup(g);
    } else {

	if (g->g_total == 1 && g->g_index == g->g_seqid) {
	    /* only one member in the group, running on the sequencer */
	    return;
	}

	if ((need_response > 0 && need_response <= (int) (g->g_total / 4)) ||
	    (want_response == 1))
	{
	    /* We only need response from a small number of members, so we
	     * unicast to each one separately to save network bandwidth.
	     */

	    for (m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
		if (m->m_state == M_UNUSED) continue;
		if (m == g->g_me) continue;
		if ((g->g_me == g->g_seq && m->m_retrial < BCNEEDRESPONSE) ||
		    (m == g->g_seq && m->m_retrial < g->g_maxretrial))
		{
		    if(fillmsg(g, &msg, (header_p) 0, BC_ALIVEREQ,
			       (g_seqcnt_t) A_BCAST, (g_msgcnt_t) 0,
			       g->g_index, (char *) 0, 0L))
		    {
			want_response--;
			STINC(st_salivereq_1);
			do_flip_unicast(g->g_ifno, msg, f, &m->m_addr,
					g->g_me->m_ep,
					(f_size_t) sizeof(bchdr_t),
					g->g_ltimeout, "checkliveness");
		    }
		}
	    }
	}

	if (want_response > 0) {
	    /* Some of the others are also somewhat late, so do a BC_ALIVEREQ
	     * broadcast to get reponse from those as well
	     */

	    if (fillmsg(g, &msg, (header_p) 0, BC_ALIVEREQ,
			(g_seqcnt_t) A_BCAST,(g_msgcnt_t) 0, g->g_index,
			(char *) 0, 0L))
	    {
		STINC(st_salivereq);
		do_flip_multicast(g->g_ifno, msg, f, &g->g_addr,
				  g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
				  MAX(g->g_total_noseq, 1), g->g_ltimeout,
				  "checkliveness");
	    }
	}
    }
}


static void store_and_send_ack(g, bc, data, n, bigendian)
    group_p g;
    bchdr_p bc;
    char *data;
    f_size_t n;
    int bigendian;
{	
    /* Sequencer will accept the msg with b_seqno, if sufficient acks have
     * come back. Store the msg in the third part of the history, because we
     * know what seqno it will get and we can't keep buffered where it is. 
     */
    int received;
    member_p src = g->g_member + bc->b_cpu;
    pkt_p msg;
    
    assert(g->g_resilience > 0);
    if(n == 0) {	/* method BB? */
	received = (bc->b_messid == src->m_bufbc.b_messid);
	if(received && ((data = src->m_bufdata) != 0)) {
	    g->g_memnbuf--;
	    src->m_bufdata = 0;
	    src->m_bufbc.b_messid = 0;
	    n = src->m_bufsize;
	    bigendian = src->m_bigendian;
	}
    } else received = 1;
    if(received) {	/* got the msg? */
	GDEBUG(LF, printf("store msg %d size %d\n", bc->b_seqno, n));
	if (hst_store(g, bc, data, n, bigendian, 0)) {
	    if (g->g_memrank <= g->g_resilience) {
		/* send ack */
		if(fillmsg(g, &msg, (header_p) 0, BC_ACK, bc->b_seqno,
			   bc->b_messid, g->g_index, (char *) 0, 0L)) {
		    STINC(st_sack);
		    do_flip_unicast(g->g_ifno, msg, 0, &g->g_seq->m_addr,
				    g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
				    g->g_ltimeout, "store_and_send_ack");
		}
	    }
	} else {
	    GDEBUG(LM, printf("store_and_ack: cannot store msg s %d m %d\n",
			      bc->b_seqno, bc->b_messid));
	}
    }
}


static void send_to_members(g, hist)
    group_p g;
    hist_p hist;
{
/* Send the members the message. */

    bchdr_p bc = &hist->h_bc;
    char *data = hist->h_data;
    f_size_t size = hist->h_size;
    member_p src = &g->g_member[bc->b_cpu];
    pkt_p msg;
    int method_PB;
    uint16 nmem;
    
    assert(bc->b_type == BC_JOINREQ || bc->b_type == BC_BCASTREQ
	   || bc->b_type == BC_LEAVEREQ);
    if(bc->b_type == BC_JOINREQ) {
	nmem = g->g_total_noseq;
	method_PB = 1;
    } else {
	nmem = (g->g_flags & FL_DESTROY) ? g->g_total : g->g_total_noseq;
	method_PB = (src == g->g_seq || size < g->g_large);
    }

    if (!method_PB) { /* data only sent in case method PB is used */
	data = 0;
	size = 0;
    }
    if (setmsg(g, &msg, (header_p) 0, bc, data, size)) {
	STINC(st_sbcast);
	do_flip_multicast(g->g_ifno, msg, 0, &g->g_addr, g->g_me->m_ep,
			  (f_size_t) (sizeof(bchdr_t) + size), MAX(nmem, 1),
			  g->g_ltimeout, "send_to_members");
    }
}


static void switchtype(bc)
    bchdr_p bc;
{
/* A message has to be made official. */

    switch(bc->b_type) {
    case BC_JOINREQ:
	bc->b_type = BC_JOIN;
	break;
    case BC_BCASTREQ:
	bc->b_type = BC_BCAST;
	break;
    case BC_LEAVEREQ:
	bc->b_type = BC_LEAVE;
	break;
    default:
	assert(bc->b_type == BC_JOIN || bc->b_type == BC_LEAVE || bc->b_type ==
	       BC_BCAST);
	break;
    }
}


static void retrial(g, cpu)
    group_p g;
    g_index_t cpu;
{
/* A retrial by cpu. If the message is in the history send it point-to-point
 * to cpu. Ignore retrials from the sequencer.
 */

    pkt_p msg;
    hist_p hist;

    if(cpu == g->g_seqid) return;
    if((hist = hst_lookup(g, cpu)) != 0) {
	if(setmsg(g, &msg, (header_p) 0, &(hist->h_bc), hist->h_data,
		  hist->h_size)) { 
	    STINC(st_sretrial);
	    do_flip_unicast(g->g_ifno, msg, 0, &g->g_member[cpu].m_addr, 
			    g->g_me->m_ep, sizeof(bchdr_t) + hist->h_size,
			    g->g_ltimeout, "retrial[1]");
	}
    } else {
	if(fillmsg(g, &msg, (header_p) 0, BC_ALIVE, 0L, 0L, g->g_index,
		   (char *) 0, 0L)) {
	    STINC(st_salive);
	    do_flip_unicast(g->g_ifno, msg, 0, &g->g_member[cpu].m_addr, 
			    g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
			    g->g_ltimeout, "retrial[2]");
	}
    }
}


/*
 * 	Functions called after receiving a group protocol message.
 */

static void processreq(g)
    group_p g;
{
/* Processreq() is the function that accepts message in the global order.
 * If no synchronization, accept the message. If it is a small message,
 * broadcast the whole message with sequence number (method PB). Otherwise, 
 * send an accept message with the sequence number (method BB). This function
 * is executed only by the sequencer.
 */
    
    hist_p hist;
    member_p src;
    pkt_p msg;
    char *buf, *p;
    uint16 nmem;
    int size;
    int method_PB;
    
    GDEBUG(LB, printf("process history (%d,%d)\n", g->g_nextseqno,
		      g->g_nexthistory));
    assert(g->g_index == g->g_seqid);
    for(hist = &g->g_history[HST_MOD(g, g->g_nextseqno)];
	g->g_nextseqno < g->g_nexthistory && hist->h_accept;
	hist = &g->g_history[HST_MOD(g, g->g_nextseqno)]) {  
	if(!HST_SYNCHRONIZE(g)) {
	    src = &g->g_member[hist->h_bc.b_cpu];
	    GDEBUG(LB, printf("processreq: a new message %d\n",
			      hist->h_bc.b_seqno));
	    assert(HST_CHECK(g));
	    g->g_nextseqno++;			/* increase sequencer number */
	    if(g->g_flags & FL_NOUSER) {
		/* no local member, so consider the msg as delivered. */
		g->g_seq->m_expect++;
	    }

	    switch(hist->h_bc.b_type) {
	    case BC_JOIN:		/* accept the member officially */
		src->m_state = M_MEMBER;    
		src->m_expect = g->g_nextseqno - 1;
		g->g_total++;
		if (src != g->g_seq) {
		    g->g_total_noseq++;
		}
		GRP_GETBUF(g, buf, g->g_maxctrlsize);
		if (buf == 0) {
		    if (hst_free_one_buf(g)) {
			GRP_GETBUF(g, buf, g->g_maxctrlsize);
		    }
		    if (buf == 0) {
			/* Shouldn't happen; the sequencer has enough buffers
			 * to let everyone join.
			 */
			GDEBUG(LR, printf("processreq: no buffer for join\n"));
			continue;
		    }
		}
		p = grp_marshallgroup(buf, buf + g->g_maxctrlsize,
				      g, hist->h_bc.b_cpu,
				      (header_p) hist->h_data);
		GRP_FREEBUF(g, hist->h_data);
		if (p == NULL) {
		    GDEBUG(LR, printf("processreq: join marshalling error\n"));
		    continue;
		}
		hist->h_data = buf;
		hist->h_size = p - buf;
		nmem = g->g_total_noseq;
		method_PB = 1;
		break;
	    case BC_LEAVE:		/* member is leaving */
		/* Do not throw the entry away; it may miss the reply and it
                 * could ask for a retransmission. Synchronize() and
                 * checkliveness() will throw the entry away, if the member
		 * is not responding.
		 */
		nmem =
		    (g->g_flags & FL_DESTROY) ? g->g_total : g->g_total_noseq;
		if(src == g->g_seq) g->g_flags |= FL_DESTROY;
		else src->m_state = M_LEAVING;
		g->g_total--;
		if (src != g->g_seq) {
		    g->g_total_noseq--;
		}
		method_PB = (src == g->g_seq || hist->h_size < g->g_large);
		/* try speeding up final removal of this member */
	        src->m_retrial = g->g_maxretrial / 2;
		break;
	    case BC_BCAST:
		nmem = g->g_total_noseq;
		if(g->g_resilience > 0) method_PB = 0;
		else {
		    method_PB = (src == g->g_seq || hist->h_size < g->g_large);
		}
		break;
	    default:
		printf("processreq: strange type %d\n", hist->h_bc.b_type);
		panic("processreq: strange type\n");
	    }

	    END_MEASURE(grp_sequencer);

	    if (method_PB) { /* method PB? */
		buf = hist->h_data;
		size = hist->h_size;
	    } else {	   /* method BB */
		buf = 0;
		size = 0;
	    }
	    if (setmsg(g, &msg, (header_p) 0, &hist->h_bc,
		       buf, (f_size_t) size)) {
		STINC(st_sbcast);
		do_flip_multicast(g->g_ifno, msg, 0, &g->g_addr, g->g_me->m_ep,
				  (f_size_t) (sizeof(bchdr_t) + size),
				  MAX(nmem, 1), g->g_ltimeout, "processreq");
	    }
	    if(g->g_flags & FL_SENDING && hist->h_bc.b_cpu == g->g_index) {
		/* wakeup sender. */
		g->g_flags &= ~FL_SENDING;
		g->g_replytimer = -1;
		wakeup((event) &g->g_sender);
	    }
	} else  {
	    GDEBUG(LM, printf("processreq: history is full\n"));
	    g->g_flags |= FL_SYNC;
	    synchronize(g, 0);
	    break;
	}
    }
}


static void processrec(g)
    group_p g;
{
/* Accept new broadcast in the history. This function is executed only by
 * regular members.
 */

    hist_p   hist;
    member_p src;
    long     lb;
    char    *buf, *p;

    GDEBUG(LB, printf("processrec: a new message (%d)\n", g->g_nextseqno));
    assert(g->g_index != g->g_seqid);
    
    for(hist = &g->g_history[HST_MOD(g, g->g_nextseqno)]; 
	g->g_nextseqno == hist->h_bc.b_seqno && hist->h_accept;
	hist = &g->g_history[HST_MOD(g, g->g_nextseqno)]) {
	src = g->g_member + hist->h_bc.b_cpu;
	assert(HST_CHECK(g));
	g->g_nexthistory++;
	g->g_nextseqno++;

	compare(g->g_minhistory, <=, g->g_me->m_expect);
	lb = (long) g->g_nexthistory - g->g_nhstfull;
	if(lb > (long) g->g_minhistory && lb <= (long) g->g_me->m_expect) {
	    g->g_minhistory = lb;
	}
	compare(g->g_minhistory, <=, g->g_me->m_expect);

	compare(hist->h_size, >=, sizeof(header));
	if (hist->h_bc.b_type != BC_JOIN) {
	    /* Trying to find out when the next assert fails. */
	    if (hist->h_bc.b_messid != src->m_messid + 1) {
		printf("%d: unexpected msg %d (%d != %d) from %d in history\n",
			group_no(g), hist->h_bc.b_type, hist->h_bc.b_messid,
			src->m_messid + 1, hist->h_bc.b_cpu);
	    }
	    compare(src->m_messid + 1, ==, hist->h_bc.b_messid);
	}
	src->m_messid = hist->h_bc.b_messid;
	GDEBUG(LB, printf("processrec: %d: mess %d\n",
			  hist->h_bc.b_cpu, src->m_messid));

	switch(hist->h_bc.b_type) {
	case BC_BCAST:
	    if(src != g->g_me)
		src->m_expect = MAX(src->m_expect, hist->h_bc.b_expect);
	    g->g_silent++;
	    if(IAMSILENT(g)) {
		sendstate(g);
		g->g_silent = WAIT_SILENT(g);
	    }
	    break;
	case BC_JOIN:
	    buf = hist->h_data + sizeof(header);
	    if(src == g->g_me) {
		/* NOTE: We have to watch out here!
		 * This join message could be for a previous member with
		 * the same member ID as us, that left soon after it joined.
		 * We have to check the group address to make sure.
		 */
		int      n;

		/* get pointer to adress of this member */
		compare(g->g_index, <, g->g_maxgroup);
		n = sizeof(adr_t) + g->g_index *
		    (2 * sizeof(adr_t) + sizeof(g_seqcnt_t) +
		     sizeof(g_msgcnt_t) + sizeof(uint32));
		if (!ADR_EQUAL((adr_p) (buf + n), &g->g_me->m_addr)) {
		    if (g->g_seq->m_vote <= (long) g->g_nextseqno) {
			/* demand a retransmission of our join, if necessary */
		        g->g_seq->m_vote = g->g_nextseqno + 2;
		    }
		    continue;
		}

		/* OK, it's really our join */
		p = grp_unmarshallgroup(buf + sizeof(adr_t),
					hist->h_data + hist->h_size,
					hist->h_bigendian, g);
		if (p == NULL) {
		    GDEBUG(LR, printf("processec: join unmarshall error\n"));
		    continue;
		}
		g->g_memrank = rank(g, g->g_index);
		g->g_nexthistory = g->g_nextseqno;
		assert(HST_CHECK(g));
		g->g_me->m_expect = g->g_nextseqno;
		g->g_flags &= ~FL_JOINING;
	    } else {		/* a new member joined */
		p = grp_unmarshallmem(buf + sizeof(adr_t),
				      hist->h_data + hist->h_size,
				      hist->h_bigendian, g, src);
		if (p == NULL) {
		    GDEBUG(LR, printf("procesrec: member unmarshall error\n"));
		    continue;
		}
		g->g_memrank = rank(g, g->g_index); /* update rank */
		g->g_total++;
		if (src != g->g_seq) {
		    g->g_total_noseq++;
		}
	    }
	    break;
	case BC_LEAVE:
	    if(g->g_me == src) { /* the ack on my leave request? */
		/* Don't throw the entry away. The leave has to be delivered
		 * to the local member. Grp_receive() will throw the entry
		 * away, when the leave is delivered.
		 */
		g->g_flags |= FL_DESTROY;
		GDEBUG(LF, printf("leaverec: destroy (me = %d, seq = %d)\n",
				  my_mem_no(g), seq_mem_no(g)));
		grp_unregister_my_ep(g);
		grp_unregister_ep(g);
		grp_unregister_ep_all(g);
		g->g_alivetimer = -1;
	    } else if(g->g_seq != src) {
		src->m_state = M_UNUSED;
	    } else {
		/* The sequencer continues to function without its
		 * local member.
		 */
		src->m_state = M_LEFTMEMBER;
	    }
	    g->g_total--;
	    if (src != g->g_seq) {
		g->g_total_noseq--;
	    }
	    g->g_memrank = rank(g, g->g_index);
	    break;
	default:
	    panic("processrec: seqno %d type == %d\n",
		  hist->h_bc.b_seqno, hist->h_bc.b_type);
	}

	if(g->g_flags & FL_SENDING && hist->h_bc.b_cpu == g->g_index) {
	    /* wakeup sender. */
	    g->g_flags &= ~FL_SENDING;
	    g->g_replytimer = -1;
	    wakeup((event) &g->g_sender);
	}
	if(g->g_flags & FL_COLLECTING) {
	    GDEBUG(LM, printf("processec: wakeup(reset)\n"));
	    wakeup((event) &g->g_reset);
	}
    }
}

#define BC_HEREIS_size	(2 * sizeof(adr_t) + 4 * sizeof(uint16) \
			 + sizeof(f_size_t) + 2 * sizeof(g_index_t))

static void bclocate(g, sa, data, bigendian)
    group_p g;
    adr_p sa;
    char *data;
    int bigendian;
{
/* Got a locate of member that is planning to join. Tell him that the
 * group exists and send him his future member id and some additional info,
 * so that it can set up its group structures (group_t, hist_t, and member_t).
 * If the member does not join in some reasonable amount of time, give
 * the member structure free again (see checkliveness() and synchronize()).
 */

    pkt_p msg;
    header_p hdr;
    g_index_t i;
    adr_p mem, rpcaddr;
    int success;
    
    hdr = (header_p) data;
    am_orderhdr(hdr, bigendian);
    rpcaddr = (adr_p) (data + sizeof(header));
    mem = (adr_p) (data + sizeof(header) + sizeof(adr_t));
    
    if (g->g_total > 0 && PORTCMP(&hdr->h_port, &g->g_port) &&
	g->g_me == g->g_seq && g->g_total < g->g_maxgroup)
    {
	if(!(success = grmem_member(g, mem, &i))) {	     /* retrial? */
	    if((success = grmem_alloc(g, &i))) { /* new member structure? */
		compare(i, <, g->g_maxgroup);
		compare(i, !=, g->g_seqid);
		grmem_new(g, i, mem, rpcaddr, 0L, g->g_nextseqno);
		g->g_member[i].m_state = M_JOINING;
	    }
	}
	if(success && fillmsg(g, &msg, (header_p) 0, BC_HEREIS, 0L, 0L, 
			      g->g_index, (char *) 0, 0L)) { 
	    STINC(st_shereis);
	    proto_dir_append(msg, &g->g_maxgroup, sizeof(uint16));
	    proto_dir_append(msg, &g->g_nloghist, sizeof(uint16));
	    proto_dir_append(msg, &g->g_logsizemess, sizeof(uint16));
	    proto_dir_append(msg, &g->g_resilience, sizeof(uint16));
	    proto_dir_append(msg, &g->g_large, sizeof(f_size_t));
	    proto_dir_append(msg, &i, sizeof(g_index_t));
	    proto_dir_append(msg, &g->g_seqid, sizeof(g_index_t));
	    proto_dir_append(msg, &g->g_prvaddr, sizeof(adr_t));
	    proto_dir_append(msg, &g->g_prvaddr_all, sizeof(adr_t));
	    do_flip_unicast(g->g_ifno, msg, 0, sa, g->g_me->m_ep,
			    (f_size_t) (sizeof(bchdr_t) + BC_HEREIS_size),
			    g->g_ltimeout, "bc_locate");
	}
    }
    GRP_FREEBUF(g, data);
}


static void bchereis(g, sa, data, n, bigendian)
    group_p g;
    adr_p sa;
    char *data;
    f_size_t n;
    int bigendian;
{
/* Got hereis; wake up thread. */

    char *p;

    if ((g->g_flags & FL_LOCATING) && (n == BC_HEREIS_size)) {
	p = data;
	g->g_maxgroup = * (uint16 *) p; p += sizeof(uint16);
	g->g_nloghist = * (uint16 *) p; p += sizeof(uint16);
	g->g_logsizemess = * (uint16 *) p; p += sizeof(uint16);
	g->g_resilience = * (uint16 *) p; p += sizeof(uint16);
	g->g_large = * (f_size_t *) p; p += sizeof(f_size_t);
	g->g_index = * (g_index_t *) p; p += sizeof(g_index_t);
	g->g_seqid = * (g_index_t *) p; p += sizeof(g_index_t);
	bc_orderhereis(bigendian, g->g_maxgroup, g->g_nloghist,
		       g->g_logsizemess, g->g_resilience,
		       g->g_large, g->g_index, g->g_seqid);
	g->g_prvaddr = * (adr_t *) p; p += sizeof(adr_t);
	g->g_prvaddr_all = * (adr_t *) p; p += sizeof(adr_t);

	GDEBUG(LM, 
	       printf("bchereis for %d; mg: %d; nh: %d; lm: %d; r: %d l: %d i: %d s: %d\n",
		      group_no(g), g->g_maxgroup, g->g_nloghist,
		      g->g_logsizemess, g->g_resilience, g->g_large, g->g_index,
		      g->g_seqid));
	flip_oneway(&g->g_prvaddr, &g->g_addr);
	g->g_addr_all = *sa;	/* use temporarily this entry */
	g->g_flags &= ~FL_LOCATING;
	g->g_flags |= FL_JOINING;
	g->g_replytimer = -1;
	wakeup((event) &g->g_sender);
    }
}



static void bcastreq(g, bc, data, n, bigendian, type)
    register group_p g;
    register bchdr_p bc;
    char *data;
    f_size_t n;
    int bigendian;
    uint8 type;
{
/* The sequencer receives a broadcast request. Update the info about the member
 * that requests the broadcast. If the message is a retrial, check the history
 * and if still available send the bcast again (the message could have been
 * purged from the history, if this is an old retrial).
 * If the history full, discard the message and synchronize. Otherwise,
 * append it to the history and process it.
 * If method BB is used, an arbitrary member can also receive this message type.
 */

    register member_p src = g->g_member + bc->b_cpu;
    hist_p hist;
    
    if(g->g_seqid != g->g_index) { 		/* method BB */
	/* Buffer message until accept from sequencer is received. */
	/* If needed, send ack to sequencer. */
	if(bc->b_seqno == 0) { 	/* msg following method BB? */
	    compare(n, >=, g->g_large);
	    compare(n, >=, sizeof(header));
	    grmem_buffer(g, src, bc, data, n, bigendian);      /* buffer msg */
	} else store_and_send_ack(g, bc, data, n, bigendian);
	return;
    }
    assert(HST_CHECK(g));
    if(src->m_expect < bc->b_expect) src->m_expect = bc->b_expect;
    if(HST_SYNCHRONIZE(g) && hst_free(g)) g->g_flags &= ~FL_SYNC;
    if(bc->b_messid <= src->m_messid) {	/* old request? */
	GDEBUG(LM, printf("bcastreq: retrial from %d mid %d\n",
			  bc->b_cpu, bc->b_messid));
	GRP_FREEBUF(g, data);
	retrial(g, bc->b_cpu);
	if (g->g_resilience > 0) {
	    hist = hst_buf_lookup(g, bc->b_cpu);
	    if (hist != 0 && !hist->h_accept) {
	        /* A retrial because the sequencer did not receive sufficient
		 * acks yet. Look the msg up in the second part and send it
		 * again to the members.
	         */
		GDEBUG(LB, printf("bcastreq: resend\n"));
		send_to_members(g, hist);
	    } else {
		/* shouldn't happen */
		if (hist == 0) {
		    GDEBUG(LB, printf("bcastreq: msg not buffered?!\n"));
		} else {
		    GDEBUG(LB, printf("bcastreq: msg already accepted?!\n"));
		}
	    }
	}
	processreq(g);
    } else {
	if(HST_FULL(g)) {
	    GRP_FREEBUF(g, data);
	    g->g_flags |= FL_SYNC;
	    GDEBUG(LM, printf("bcastreq: history is full\n"));
	    synchronize(g, 0);
	} else { /* append message to history */
	    compare(src->m_messid + 1, ==, bc->b_messid);
	    src->m_messid = bc->b_messid;
	    GDEBUG(LB, printf("bcastreq: %d: mess %d\n",
				bc->b_cpu, src->m_messid));
	    bc->b_type = type;
	    bc->b_seqno = g->g_nexthistory;
	    hist = hst_append(g, bc, data, n, bigendian);
	    if(MIN((int) g->g_total-1, (int) g->g_resilience) > 0) {
		send_to_members(g, hist);
	    } else hist->h_accept = 1;
	    processreq(g);
	}
    }
}


static void bcastrec(g, bc, data, n, bigendian)
    register group_p g;
    register bchdr_p bc;
    char *data;
    f_size_t n;
    int bigendian;
{
/* A broadcast from the sequencer arrives. In method PB, this message contains
 * the bcastreq message. In method BB, the bcastreq message is buffered.
 * If a member has been silent for a long time, it sends the
 * sequencer a state message to avoid that the history gets full.
 */
    int received;
    int data_bigendian = bigendian;
    register member_p src;
    hist_p hist;
    
    src = g->g_member + bc->b_cpu;
    if(n == 0) {		/* Method BB? */
	hist = &g->g_history[HST_MOD(g, bc->b_seqno)];
	if(g->g_resilience > 0 && hist->h_bc.b_messid == bc->b_messid) {
	    /* Message is already stored in the third part of the history. */
	    GDEBUG(LF, printf("bcastrec: msg is already in history\n"));
	    compare(hist->h_bc.b_messid, ==, bc->b_messid);
	    compare(hist->h_bc.b_cpu, ==, bc->b_cpu);
	    compare(hist->h_size, >=, sizeof(header));
	    hist->h_bc = *bc;
	    hist->h_accept = 1;
	    processrec(g);
	    return;
	}
	received = (bc->b_messid == src->m_bufbc.b_messid);
	if(received && ((data = src->m_bufdata) != 0)) {
	    /* The header in a buffered message is already ordered: */
	    GDEBUG(LF, printf("bcastrec: msg is already in buffer\n"));
	    g->g_memnbuf--;
	    src->m_bufdata = 0;
	    src->m_bufbc.b_messid = 0;
	    n = src->m_bufsize;
	    data_bigendian = src->m_bigendian;
	}
    } else received = 1;
    if(g->g_nextseqno == bc->b_seqno && received) { /* accept it? */
	GDEBUG(LF, printf("bcastrec: msg s %d m %d\n",
			  bc->b_seqno, bc->b_messid));
	if (hst_store(g, bc, data, n, data_bigendian, 1)) {
	    processrec(g);
	    if(g->g_seq->m_vote >= (long) g->g_nextseqno) {
		GDEBUG(LM, printf("bcastrec: seqvote %d nextseq %d: retrans\n",
				  g->g_seq->m_vote, g->g_nextseqno));
		ask_for_retrans(g, g->g_seqid,
				(g_seqcnt_t) (g->g_seq->m_vote - 1)); 
	    }
	} else {
	    GDEBUG(LM, printf("bcastrec: cannot store msg s %d m %d\n",
			      bc->b_seqno, bc->b_messid));
	}
    }
    else if(g->g_nextseqno < bc->b_seqno || (g->g_nextseqno == bc->b_seqno
					     && !received)) {
	GDEBUG(LM, printf("bcastrec: msg out of order %d, %d\n", bc->b_seqno,
			  g->g_nextseqno));
	if(received) {
	    if (hst_store(g, bc, data, n, data_bigendian, 1)) {
		ask_for_retrans(g, g->g_seqid, bc->b_seqno - 1);
	    } else {
		GDEBUG(LM, printf("bcastrec: cannot store msg s %d m %d\n",
				  bc->b_seqno, bc->b_messid));
	    }
	} else {
	    ask_for_retrans(g, g->g_seqid, bc->b_seqno - 1);
	}
    } else {			/* old message */
	GDEBUG(LM, printf("bcastrec: discard old msg %d\n", bc->b_seqno));
	GRP_FREEBUF(g, data);
    }

    /* Since we got a message from the sequencer, we know it's still alive.
     * Updating the retrial counter here may avoid unnecessary group aborts
     * in case the group is very busy and one of the timers is configured
     * somewhat too tight.
     */
    if (g->g_seq->m_retrial < BCNEEDRESPONSE) {
	g->g_seq->m_retrial = BCNEEDRESPONSE;
    }
}


#define BIT(mask, n)	((mask >> n) & 0x1)


static void ackrec(g, bc)
    group_p g;
    bchdr_p bc;
{
    hist_p hist;
    g_indx_t r;			/* rank of the sender */
    
    assert(g->g_index == g->g_seqid);
    hist = &g->g_history[HST_MOD(g, bc->b_seqno)];
    GDEBUG(LB, printf("ack from %d for seqno %d(%d)\n", bc->b_cpu, bc->b_seqno,
		      bc->b_messid));
    compare(hist->h_bc.b_messid, ==, bc->b_messid);
    r = rank(g, bc->b_cpu);
    if(!BIT(hist->h_mem_mask, r)) {
	hist->h_mem_mask |= (0x1 << r);
	hist->h_nack++;
    }
    if(hist->h_nack >= MIN((int) g->g_total-1, (int) g->g_resilience) &&
       !hist->h_accept) {
	hist->h_accept = 1;
	switchtype(&hist->h_bc);
	processreq(g);
    }
}


static void retransreq(g, bc)
    group_p g;
    bchdr_p bc;
{
/* Retransmission request. If the requested message is in the history, send
 * point-to-point the message.
 */

    hist_p hist;
    member_p src;
    pkt_p msg;
    
    src = &g->g_member[bc->b_cpu];
    /* If the joining member missed the join message, it doesn't know
     * the sequence number of the message it has to ask for.
     */
    if (bc->b_seqno < src->m_expect) {
	GDEBUG(LM, printf("retrans req: assume %d is wanted, not %d\n",
			  src->m_expect, bc->b_seqno));
	bc->b_seqno = src->m_expect;
    }
    GDEBUG(LF, printf("(%d,%d) retransreq for %d by %d\n", g->g_minhistory,
		      g->g_nextseqno, bc->b_seqno, bc->b_cpu));

    if (src->m_retrial >= 0) {	/* mark this member alive */
	src->m_retrial = g->g_maxretrial;
	src->m_replied = 1;
    }

    if(HST_IN(g, bc->b_seqno)) { 
	if(g->g_index == g->g_seqid && src->m_expect < bc->b_expect) {
	    src->m_expect = bc->b_expect;
	    if(HST_SYNCHRONIZE(g) && hst_free(g)) {
		g->g_flags &= ~FL_SYNC;
		processreq(g);
	    }
	}
	hist = &g->g_history[HST_MOD(g, bc->b_seqno)];
	compare(hist->h_bc.b_seqno, == , bc->b_seqno);
	if(setmsg(g, &msg, (header_p) 0, &(hist->h_bc), hist->h_data,
		  hist->h_size)) { 
	    STINC(st_sretrans);
	    do_flip_unicast(g->g_ifno, msg, 0, &src->m_addr, g->g_me->m_ep, 
			    sizeof(bchdr_t) + hist->h_size, g->g_ltimeout,
			    "retransreq");
	}
    }
    if(g->g_flags & FL_COORDINATOR) {
	src->m_vote = 1;
	src->m_retrial = g->g_maxretrial;
    }
}


static void syncreq(g, bc, data, n, bigendian)
    group_p g;
    bchdr_p bc;
    char *data;
    f_size_t n;
    int bigendian;
{
/* A sychronization request by the sequencer. If a member is up-to-date, it
 * sends back a state message. If it is behind, it asks for retransmission(s).
 * GRP_FREEBUF() is called before a msg is sent. This message may introduce
 * recursion, which gives problems with global bookkeeping of buffer allocation
 * (g_nbuf, g_nhistbuf, etc.).
 */

    f_size_t i, cnt;
    g_index_t mem;
    char *p, *end;
    
    assert(n > 0);
    p = data;
    end = data + n;
    cnt = n / sizeof(g_index_t);
    if(g->g_replytimer > 0) {
	settimer(group_no(g));
    }
    /* we know that the sequencer is alive; see comment in bcastrec */
    if (g->g_seq->m_retrial < BCNEEDRESPONSE) {
	g->g_seq->m_retrial = BCNEEDRESPONSE;
    }
    for (i = 0; i < cnt; i++) {
    	p = grp_unmarshall_uint16(p, end, &mem);
	if (p == NULL) {
	    GDEBUG(LR, printf("syncreq: unmarshall error\n"));
	    break;
	}
	bc_orderindex(bigendian, mem);
	if(mem == g->g_index) {
	    GRP_FREEBUF(g, data);
	    if(g->g_nextseqno >= bc->b_seqno) {
		/* I am up-to-date, but the sequencer didn't know */
		GDEBUG(LM, printf("syncreq: %d already up-to-date (%d)\n",
				  g->g_index, g->g_nextseqno));
		sendstate(g);
		/* Here we set g_silent to 0 rather than WAIT_SILENT
		 * in order to spread out future state messages from
		 * the various members.
		 */
		g->g_silent = 0;
	    } else {
		g->g_seq->m_vote = bc->b_seqno;
		GDEBUG(LM, printf("syncreq: nextseq %d b_seq %d: retrans\n",
				  g->g_nextseqno, bc->b_seqno));
		ask_for_retrans(g, g->g_seqid, bc->b_seqno - 1);
	    }
	    return;
	}
    }
    GDEBUG(LM, printf("syncreq: not meant for %d\n", g->g_index));
    GRP_FREEBUF(g, data);
}


static void stateinfo(g, bc)
    group_p g;
    bchdr_p bc;
{
/* Receive state information from a member.  */

    member_p src;
    
    src = g->g_member + bc->b_cpu;
    src->m_vote = bc->b_seqno;
    if (src->m_retrial >= 0) {	/* mark this member alive */
	src->m_retrial = g->g_maxretrial;
	src->m_replied = 1;
    }
    if(HST_IN(g, bc->b_expect) || g->g_nextseqno <= bc->b_expect) {
	assert(HST_CHECK(g));
	if(src->m_expect  < bc->b_expect) {
	    src->m_expect = bc->b_expect;
	    if(HST_SYNCHRONIZE(g) && hst_free(g)) {
		g->g_flags &= ~FL_SYNC;
		processreq(g);
	    }
	}
    }
}



static void alive(g, bc)
    group_p g;
    bchdr_p bc;
{
/* Receive alive message from a member. */

    GDEBUG(LM, printf("%d is still alive\n", bc->b_cpu));
    g->g_member[bc->b_cpu].m_retrial = g->g_maxretrial;
    g->g_member[bc->b_cpu].m_replied = 1;
}


/* Receive a dead message from the kernel that ran the now dead member. */
static void dead(g, bc)
    group_p g;
    bchdr_p bc;
{
/* Somebody died. */

    GDEBUG(LM, printf("dead: %d died\n", bc->b_cpu));
    g->g_member[bc->b_cpu].m_retrial = -1;
    switchstate(g);
}


static void alivereq(g, bc)
    group_p g;
    bchdr_p bc;
{
/* Are you alive? */

    pkt_p msg;
    int ep = -1;

    g->g_member[bc->b_cpu].m_retrial = g->g_maxretrial;
    g->g_member[bc->b_cpu].m_replied = 1;
    if(g->g_flags & FL_RESET) {
	ep = g->g_me->m_ep;
    } else {	/* everything is fine */
	if (bc->b_seqno == A_BCAST) {
	    if(bc->b_cpu == g->g_seqid) {
		assert(g->g_seqid != g->g_index);
		/* Make sure that the destination knows that this member is
		 * listening to the group address.
		 */
		ep = g->g_ep;
	    } else {
		ep = g->g_ep_all;
	    }
	} else {
	    ep = g->g_me->m_ep;
	}
    }

    if (ep >= 0) {
	if (fillmsg(g, &msg, (header_p) 0, BC_ALIVE, (g_seqcnt_t) 0,
		    (g_msgcnt_t) 0, g->g_index, (char *) 0, (f_size_t) 0)) { 
	    STINC(st_salive);
	    do_flip_unicast(g->g_ifno, msg, 0, &g->g_member[bc->b_cpu].m_addr, 
			    ep, (f_size_t) sizeof(bchdr_t),
			    g->g_ltimeout, "alivereq");
	}
    }
}


static int stay_coordinator(g, seqno, cpu) 
    group_p g;
    g_seqcnt_t seqno;
    g_index_t cpu;
{
    if (seqno < g->g_nextseqno) {
	/* other member has a lower seqno */
	return 1;
    } else if (seqno > g->g_nextseqno) {
	/* other member has a higher seqno */
	return 0;
    } else {
	/* sequence numbers are equal; let member with lowest index win */
	return g->g_index < cpu;
    }
}


static void reformreq(g, bc)
    group_p g;
    bchdr_p bc;
{
/* A member thinks that another member has died. It asks us if we agree and if
 * we want recover. If we are also trying to recover the group, it has to be
 * decided who is going to do the job. stay_coordinator() makes the decision.
 */

    int sequenceralive = (g->g_me == g->g_seq);
    pkt_p msg;
    g_seqcnt_t vote_seq;
    
    GDEBUG(LM, printf("reformreq(%d) from %d\n", group_no(g), bc->b_cpu));
    if (sequenceralive && (g->g_flags & FL_NOUSER)) {
	/* A new group is built, we might as well skip out now. */
	deletegroup(g);
	return;
    }
    g->g_member[bc->b_cpu].m_retrial = g->g_maxretrial;
    if ((g->g_flags & FL_RESULT) ||
	(g->g_coord == g->g_me && stay_coordinator(g, bc->b_seqno, bc->b_cpu)))
    { 
	vote_seq = STOP;
    } else if (sequenceralive && stay_coordinator(g, bc->b_seqno, bc->b_cpu)) {
	/* Note: the fact that we are the sequencer doesn't rule out the
	 * possibility that there's another member with the same next
	 * sequence number.  If that member has lower index, it should be
	 * the one to become the coordinator.  Otherwise the following
	 * could happen:
	 * 	member 1 (nextseq = 12345)	      -> vote STOP
	 *	member 2 (nextseq = 12345; sequencer) -> vote ABORT
	 * i.e., deadlock because both members think the other is coordinator.
	 */
	vote_seq = ABORT;
    } else {
	switchstate(g);
	g->g_flags |= FL_VOTE;
	g->g_coord = &g->g_member[bc->b_cpu];
	if(g->g_flags & FL_COORDINATOR) {
	    g->g_flags &= ~FL_COORDINATOR;
	    g->g_flags |= FL_COHORT;
	}
	vote_seq = g->g_nextseqno;
    }

    if (fillmsg(g, &msg, (header_p) 0, BC_VOTE, vote_seq,
		(g_msgcnt_t) 0, g->g_index, (char *) 0, (f_size_t) 0)) {
	STINC(st_svote);
	do_flip_unicast(g->g_ifno, msg, 0, &g->g_member[bc->b_cpu].m_addr, 
			g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
			g->g_ltimeout, "reformreq");
    }
}


static void vote(g, bc)
    group_p g;
    bchdr_p bc;
{
/* A vote arrived. */
    GDEBUG(LM, printf("vote(%d): %d voting %d\n", group_no(g),
		      bc->b_cpu, bc->b_seqno));

    if(g->g_flags & FL_VOTE && g->g_coord == g->g_me) {
	g->g_member[bc->b_cpu].m_vote = bc->b_seqno;
	g->g_member[bc->b_cpu].m_expect = bc->b_expect;
	g->g_member[bc->b_cpu].m_retrial = g->g_maxretrial;
	g->g_member[bc->b_cpu].m_replied = 1;
	if ((g->g_flags & FL_VOTE) &&
	    (bc->b_seqno == ABORT || bc->b_seqno == STOP))
	{ 
	    GDEBUG(LM, printf("vote(%d): voting is finished; %d takes over\n",
			      group_no(g), bc->b_cpu));
	    g->g_flags &= ~FL_COORDINATOR;
	    g->g_flags |= FL_COHORT;
	    g->g_coord = &g->g_member[bc->b_cpu];
	    wakeup((event) &g->g_reset);
	} else if(votefinished(g)) {
	    GDEBUG(LM, printf("vote(%d): voting is finished\n", group_no(g)));
	    g->g_flags &= ~FL_VOTE;
	    wakeup((event) &g->g_reset);	
	}
    } else {
	GDEBUG(LM, printf("vote ignored\n"));
    }
}


static void result(g, bc, data, n, bigendian)
    group_p g;
    bchdr_p bc;
    char *data;
    f_size_t n;
    int bigendian;
{
/* The coordinator sends us the result of the recovery. */

    g_seqcnt_t seqno;
    pkt_p msg;
    char *p;
    int i;
    
    seqno = bc->b_seqno;
    GDEBUG(LM, printf("result(%d): new seq is %d(%d)\n", group_no(g),
		      coord_mem_no(g), seqno)); 
    if(g->g_flags & FL_VOTE) {
	if(seqno == g->g_nextseqno) {
	    GDEBUG(LM, printf("result(%d): accept reset\n", group_no(g)));
	    p = grp_unmarshall_result(data + sizeof(header), data + n,
				      bigendian, g);
	    if (p == NULL) {
		GDEBUG(LR, printf("result: unmarshall error\n"));
		GRP_FREEBUF(g, data);
		return;
	    }
	    /* For the alive members, reset retry counters. */
	    for(i = 0; i < (int) g->g_maxgroup; i++)  {
		if(IS_MEMBER(g->g_member[i].m_state))  {
		    g->g_member[i].m_replied = 1;
		    g->g_member[i].m_retrial = g->g_maxretrial;
		}
	    }
	    (void) hst_append(g, bc, data, n, bigendian);
	    g->g_member[bc->b_cpu].m_messid++;
	    GDEBUG(LB, printf("result: %d: mess %d\n",
				bc->b_cpu, g->g_member[bc->b_cpu].m_messid));
	    g->g_nextseqno++;
	    g->g_flags &= ~FL_RESET;
	    g->g_flags &= ~FL_VOTE;
	    g->g_memrank = rank(g, g->g_index);
	    flip_oneway(&g->g_prvaddr, &g->g_addr);
	    flip_oneway(&g->g_prvaddr_all, &g->g_addr_all);
	    if((g->g_ep = flip_register(g->g_ifno, &g->g_prvaddr)) < 0)
		panic("result: flip_register failed");
	    if((g->g_ep_all = flip_register(g->g_ifno, &g->g_prvaddr_all)) < 0)
		panic("result: flip_register failed");
	    GDEBUG(LM, grp_dump((char *) 0, (char *) 0));
	    if(fillmsg(g, &msg, (header_p) 0, BC_RESULTACK, 0L, 0L,
		      g->g_index, (char *) 0, 0L)) {
		STINC(st_sresultack);
		do_flip_unicast(g->g_ifno, msg, 0,
				&g->g_member[bc->b_cpu].m_addr, 
				g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
				(interval) 0, "result[1]");
	    }
	    if(g->g_flags & FL_COHORT) {
		GDEBUG(LM, printf("result: wakeup(reset)\n"));
		wakeup((event) &g->g_reset);
	    }
	    if(g->g_flags & FL_SENDING) {
		g->g_flags |= FL_FAILED;
		g->g_flags &= ~FL_SENDING;
		wakeup((event) &g->g_sender);
	    }
	    setalivetimer(g);
	}
	else if(seqno > 0) {
	    GDEBUG(LM, printf("result(%d): still behind\n", group_no(g)));
	    GRP_FREEBUF(g, data);
	    ask_for_retrans(g, (g_index_t) coord_mem_no(g), g->g_nextseqno);
	}
	else {				/* recovery failed */
	    GDEBUG(LM, printf("result(%d): recovery failed\n", group_no(g)));
	    GRP_FREEBUF(g, data);
	    g->g_flags &= ~FL_VOTE;
	    if(fillmsg(g, &msg, (header_p) 0, BC_RESULTACK, 0L, 0L, 
		      g->g_index, (char *) 0, 0L)) {
		STINC(st_sresultack);
		do_flip_unicast(g->g_ifno, msg, 0,
				&g->g_member[bc->b_cpu].m_addr, 
				g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
				g->g_ltimeout, "result[2]");
	    }
	    if(g->g_flags & FL_COHORT) {
		GDEBUG(LM, printf("result[2]: wakeup(reset)"));
		wakeup((event) &g->g_reset);
	    }
	}
    }
    else {
	GRP_FREEBUF(g, data);
    }
}


static void resultack(g, bc)
    group_p g;
    bchdr_p bc;
{
/* A member tells us that it has received the result message. After receiving
 * an ack from all new members, wakeup the coordinator.
 */

    GDEBUG(LM, printf("resultack(%d): %d is back to normal\n",
		      group_no(g), bc->b_cpu));
    g->g_member[bc->b_cpu].m_replied = 1;
    if(repliedfinished(g)) {
	g->g_flags &= ~FL_RESULT;
	GDEBUG(LM, printf("resultack: wakeup(reset)"));
	wakeup((event) &g->g_reset);
    }
}


static void grp_switch(g, src, bc, data, n, bigendian)
    register group_p g;
    adr_p src;
    register bchdr_p bc;
    char *data;
    f_size_t n;
    int bigendian;
{
/* A broadcast-protocol message has arrived. If certain conditions are met
 * throw the message away (e.g., a message from a previous group incarnation.
 * Otherwise, run the protocol.
 */
    g_index_t i;

    GDEBUG(LF, { printf("%d: rcv %d: ", group_no(g), n); 
		 bc_print(bc); printf("\n"); } );
    GDEBUG(LF, { printf("    %d: ", group_no(g));
		 (void) grp_dumpflag((char *) 0, (char *) 0, g->g_flags); } );

    /* HACK; for these types the incarnation number does not matter. */
    if(bc->b_type == BC_LOCATE || bc->b_type == BC_HEREIS ||
       bc->b_type == BC_JOINREQ) bc->b_incarnation = g->g_incarnation;

    /* Discard packets. If the member is joining (i.e. FL_LOCATING or
     * FL_JOINING is set), it can only accept a BC_HEREIS or a BC_JOIN.
     * If the group is in recovery mode, accept only messages that are needed
     * to recover. If the group is in normal, accept only message from this
     * this incarnation.
     */
    if((g->g_flags & FL_JOINING && bc->b_type != BC_JOIN) || 
       (g->g_flags & FL_LOCATING && bc->b_type != BC_HEREIS) || 
       (g->g_flags & FL_RESET && bc_request(bc->b_type)) ||
       (!(g->g_flags & (FL_RESET | FL_JOINING)) && 
	bc->b_incarnation > g->g_incarnation)) {
	GDEBUG(LM, printf("grp_switch(%d): bad message; discard %d\n", 
			  group_no(g), bc->b_type));
	GDEBUG(LM, printf("b_incarnation %d; g_incarnation %d, flags = %x\n", 
			  bc->b_incarnation, g->g_incarnation, g->g_flags));
			  
	if(bc->b_type == BC_ALIVE && g->g_flags & FL_JOINING) {
	    /* The sequencer is there, but it can not acknowledge the join
	     * yet.
	     */
	    g->g_seq->m_retrial = g->g_maxretrial;
	}
	if(bc->b_type != BC_HEREIS) GRP_FREEBUF(g, data);
	return;
    }
    assert(((n > 0 && (bc->b_type != BC_HEREIS)) ? 1 : 0) + g->g_memnbuf +
	g->g_hstnbuf + g->g_bufnbuf == g->g_nbuf);
    switch(bc->b_type) {
    case BC_LOCATE:
	STINC(st_rlocate);
	bclocate(g, src, data, bigendian);
	break;
    case BC_HEREIS:
	STINC(st_rhereis);
	bchereis(g, src, data, n, bigendian);
	break;
    case BC_JOINREQ:
	STINC(st_rjoinreq);
	if ((((g->g_member[bc->b_cpu].m_state == M_JOINING || 
	       IS_MEMBER(g->g_member[bc->b_cpu].m_state)) && 
	      grmem_member(g, src, &i) && i == bc->b_cpu))	||
	    ((g->g_index != g->g_seqid &&
	      IS_MEMBER(g->g_member[g->g_index].m_state))))
	{
	    bcastreq(g, bc, data, n, bigendian,
		  (uint8) ((MIN((int) g->g_resilience,(int)g->g_total-1) == 0 ?
				BC_JOIN : BC_JOINREQ)));
	} else GRP_FREEBUF(g, data);
	break;
    case BC_JOIN:
	STINC(st_rjoin);
	if(bc->b_cpu == g->g_index && g->g_flags & FL_JOINING) {
	    /* The ack on my join request.
	     * If bc->b_seqno < g->g_nextseqno, assume that it is a
	     * retransmission of an old join which we want to skip,
	     * so don't touch the history pointers in that case.
	     */
	    GDEBUG(LM, printf("got BC_JOIN, seqno = %d, nextseqno = %d\n",
			      bc->b_seqno, g->g_nextseqno));
	    if (bc->b_seqno >= g->g_nextseqno) {
	        g->g_minhistory = bc->b_seqno;
	        g->g_nextseqno = bc->b_seqno;
	        g->g_nexthistory = bc->b_seqno;
	        g->g_me->m_expect = bc->b_seqno;
	    }
	    bcastrec(g, bc, data, n, bigendian);
	} else if(!(g->g_flags & FL_JOINING)) {
	    /* We are listening to the group addresses and we are not joining,
	     * this means that we are a member and can accept the join.
	     */
	    bcastrec(g, bc, data, n, bigendian);
	} else {
	    GDEBUG(LM, printf("grp_switch(JOIN): %d ignored\n", bc->b_cpu));
	    GRP_FREEBUF(g, data);
	}
	break;
    case BC_LEAVEREQ:
	STINC(st_rleavereq);
	if((ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) ||
	    ADR_EQUAL(&g->g_seq->m_addr, src)) &&
	   ((IS_MEMBER(g->g_member[bc->b_cpu].m_state) ||
	     g->g_member[bc->b_cpu].m_state == M_LEAVING))) { 
	    bcastreq(g, bc, data, n, bigendian,
		   (uint8) ((MIN((int)g->g_resilience,(int)g->g_total-1) == 0 ?
				BC_LEAVE : BC_LEAVEREQ)));
	} else GRP_FREEBUF(g, data);
	break;
    case BC_LEAVE:
	if(IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rleave);
	    bcastrec(g, bc, data, n, bigendian);
	} else GRP_FREEBUF(g, data);
	break;
    case BC_BCASTREQ:
	if(IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rbcastreq);
	    bcastreq(g, bc, data, n, bigendian,
		   (uint8) ((MIN((int)g->g_resilience,(int)g->g_total-1) == 0 ?
				BC_BCAST : BC_BCASTREQ)));
	} else {
	    GDEBUG(LM, printf("grp_switch(BCASTREQ): %d not a member\n",
			      bc->b_cpu));
	    GRP_FREEBUF(g, data);
	}
	break;
    case BC_BCAST:
	if(IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rbcast);
	    bcastrec(g, bc, data, n, bigendian);
	} else {
	    GDEBUG(LM, printf("grp_switch(BCAST): %d not a member\n",
			      bc->b_cpu));
	    GRP_FREEBUF(g, data);
	}
	break;
    case BC_ACK:
	assert(n == 0);
	if(ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) && 
	   IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rack);
	    ackrec(g, bc);
	}
	break;
    case BC_RETRANS:
	assert(n == 0);
	if(ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) &&
	   ((IS_MEMBER(g->g_member[bc->b_cpu].m_state) ||
	     g->g_member[bc->b_cpu].m_state == M_LEAVING))) {
	    STINC(st_rretrans);
	    retransreq(g, bc);
	}
	break;
    case BC_SYNC:
	if(IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rsync);
	    syncreq(g, bc, data, n, bigendian);
	} else GRP_FREEBUF(g, data);
	break;
    case BC_STATE:
	assert(n == 0);
	if(ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) && 
	   (IS_MEMBER(g->g_member[bc->b_cpu].m_state) ||
	    g->g_member[bc->b_cpu].m_state == M_LEAVING)) {
	    STINC(st_rstate);
	    stateinfo(g, bc);
	}
	break;
    case BC_ALIVE:
	assert(n == 0);
	if(IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_ralive);
	    alive(g, bc);
	}
	break;
    case BC_ALIVEREQ:
	assert(n == 0);
	if(IS_MEMBER(g->g_member[bc->b_cpu].m_state) ||
	   g->g_member[bc->b_cpu].m_state == M_LEAVING) {
	    STINC(st_ralivereq);
	    alivereq(g, bc);
	}
	break;
    case BC_REFORMREQ:
	assert(n == 0);
	GDEBUG(LM, printf("reform req received for %d: %d -> %d\n",
			  group_no(g), bc->b_cpu, bc->b_seqno));
	if(ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) && 
	   IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rreformreq);
	    reformreq(g, bc);
	}
	break;
    case BC_VOTE:
	assert(n == 0);
	GDEBUG(LM, printf("vote received for %d: %d vote %d\n",
			  group_no(g), bc->b_cpu, bc->b_seqno));
	if(ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) && 
	   IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rvote);
	    vote(g, bc);
	}
	break;
    case BC_RESULT:
	if(IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rresult);
	    result(g, bc, data, n, bigendian);
	} else GRP_FREEBUF(g, data);
	break;
    case BC_RESULTACK:
	assert(n == 0);
	if(ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) && 
	   IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rresultack);
	    resultack(g, bc);
	}
	break;
    case BC_DEAD:
	assert(n == 0);
	if(ADR_EQUAL(&g->g_member[bc->b_cpu].m_addr, src) && 
	   IS_MEMBER(g->g_member[bc->b_cpu].m_state)) {
	    STINC(st_rdead);
	    dead(g, bc);
	}
	break;
    default:
	if(n > 0) GRP_FREEBUF(g, data);
	printf("grp_switch: Illegal bcast type %d\n", bc->b_type);
    }
    assert(g->g_memnbuf + g->g_hstnbuf + g->g_bufnbuf == g->g_nbuf);
    if((g->g_flags & FL_RECEIVING) && HST_IN(g, g->g_me->m_expect)){
	wakeup((event) &g->g_receiver);
    }
    assert(g->g_memnbuf + g->g_hstnbuf + g->g_bufnbuf == g->g_nbuf);
}

/* To speed up finding the member id of the originator in general
 * we could also create a hash table on the source address.  Later.
 */
#define GRP_FIND_MEMBER(g, src, m) \
    if (g->g_last_sender != 0 && ADR_EQUAL(src, &g->g_last_sender->m_addr)) { \
	m = g->g_last_sender;						\
    } else {								\
	register member_p end = &g->g_member[g->g_maxgroup];		\
									\
	for (m = g->g_member; m < end; m++) {				\
	    if (ADR_EQUAL(src, &m->m_addr))				\
		break;							\
	}								\
	if (m < end) {							\
	    g->g_last_sender = m;					\
	} else {							\
	    m = 0;							\
	}								\
    }

static void grp_data(g, src, messid, offset, length, total, pkt)
    group_p g;
    adr_p src;
    f_msgcnt_t messid;
    f_size_t offset, length, total;
    pkt_p pkt;
{
/* A fragment of a flip message has arrived. Try to reassemble it. */

    register member_p m;
    char *d;
    int bigendian;
    
    assert(pkt->p_contents.pc_virtual == 0);
    GRP_FIND_MEMBER(g, src, m);
    if (m == 0) {
	return;
    }
    if(offset != m->m_roffset || messid != m->m_rmid) {
	GDEBUG(LF, printf("assemble %d(%d)  new fragment %d(%d)\n", m->m_rmid,
	       m->m_roffset, messid, offset));
	return;
    }
    if (total != m->m_total || offset + length > total) {
	/* shouldn't happen; indicates message corruption */
	GDEBUG(LR, printf("grp_data: m_total %d total %d off %d len %d\n",
			  m->m_total, total, offset, length));
	return;
    }
    GDEBUG(LF, printf("%d(%d): reassemble %d(%d), %d\n", group_no(g),
		      mem_no(g, m), m->m_rmid, offset, length));
    memmove((_VOIDSTAR) (m->m_data + m->m_roffset - sizeof(bchdr_t)),
		(_VOIDSTAR) pkt_offset(pkt), (size_t) length);
    m->m_roffset += length;
    if(offset + length == total) {
	m->m_roffset = 0;
	m->m_total = 0;
	d = m->m_data;
	m->m_data = 0;
	g->g_memnbuf--;
	bigendian = m->m_bc.b_flags & F_BIGENDIAN;
	bc_orderhdr(&m->m_bc, bigendian);
	grp_switch(g, (adr_p) 0, &m->m_bc, d, total - sizeof(bchdr_t),
		   bigendian); 
    }
    
}


static void grp_reassemble(g, src, messid, length, total, pkt)
    group_p g;
    adr_p src;
    f_msgcnt_t messid;
    f_size_t length, total;
    pkt_p pkt;
{
/* The first fragment of a flip message has arrived. Store it and do
 * initialize some bookeeping information.
 */

    register member_p m;
    bchdr_p bc;
    
    PROTO_LOOK_HEADER(bc, pkt, bchdr_t);
    if(bc == 0) return;
    if (total > g->g_maxsizemess && total > g->g_maxctrlsize) {
	return;
    }
    GRP_FIND_MEMBER(g, src, m);
    if (m == 0) {
	return;
    }
    if(m->m_data) {
	GDEBUG(LF, printf("old message %d(%d) new message %d\n", m->m_rmid,
	       m->m_roffset, messid));
	g->g_memnbuf--;
	GRP_FREEBUF(g, m->m_data);
    }
    m->m_bc = *bc;
    proto_remove_header(pkt);
    GRP_GETBUF(g, m->m_data, total);
    if (m->m_data == 0) {
	if (hst_free_one_buf(g)) {
	    GRP_GETBUF(g, m->m_data, total);
	}
	if (m->m_data == 0) {
	    /* This could potentially happen when a non-sequencer member
	     * is running far behind, and is using all buffers for the
	     * unprocessed messages in history.  We could try to to purge
	     * the newest entries from the buffer in that case, and ask
	     * the sequencer for retransmissions later.
	     */
	    GDEBUG(LR, printf("grp_reassemble: no buffer available\n"));
	    return;
	}
    }
    g->g_memnbuf++;
    GDEBUG(LF, printf("%d(%d): reassemble %d(%d)\n", group_no(g), 
		      mem_no(g, m), messid, length));
    memmove((_VOIDSTAR) m->m_data, (_VOIDSTAR) pkt_offset(pkt),
							    (size_t) length);
    m->m_roffset = length;
    m->m_total = total;
    m->m_rmid = messid;
}


static int split(g, pkt, data, n)
    group_p g;
    pkt_p pkt;
    char **data;
    f_size_t *n;
{
/* A broadcast-protocol message has arrived. Get a data buffer and copy
 * user data into it.
 */
    register f_size_t totsize;

    *n = totsize = pkt->p_contents.pc_totsize;
    if (totsize > g->g_maxsizemess && totsize > g->g_maxctrlsize) {
	printf("grp_split: packet size (%d) > g_maxsizemess (%d)\n",
	       totsize, g->g_maxsizemess);
	return 0;
    }
    if (totsize > 0) {
	char *buf;

	GRP_GETBUF(g, buf, totsize);
	if (buf == 0) {
	    if (hst_free_one_buf(g)) {
		GRP_GETBUF(g, buf, totsize);
	    }
	    if (buf == 0) {
		/* Out of message buffers; see comment in grp_reassemble */
		GDEBUG(LR, printf("split: no buffer available\n"));
		return 0;
	    }
	}
	memmove((_VOIDSTAR) buf, (_VOIDSTAR) pkt_offset(pkt),
		(size_t) pkt->p_contents.pc_dirsize);
	if (pkt->p_contents.pc_dirsize != totsize)  {
	    memmove((_VOIDSTAR) (buf + pkt->p_contents.pc_dirsize),
		    (_VOIDSTAR) pkt->p_contents.pc_virtual,
		    (size_t)(totsize - pkt->p_contents.pc_dirsize));
	}
	*data = buf;
    } else {
	*data = 0;
    }
    return 1;
}


/*ARGSUSED*/
static void grp_arrived(g, pkt, dst, src, messid, offset, length, total)
    group_p g;
    pkt_p pkt;
    adr_p dst, src;
    f_msgcnt_t messid;
    f_size_t offset, length, total;
{
/* A message from the flip layer arrives. Run sanity checks and pass it to
 * the appropriate routine.
 * For messages passed to grp_switch(), grp_arrived breaks the message in
 * pieces (protocol header, and data; see split()), copies the data in a group
 * buffer (the buffer will most likely be stored in the history).
 * (BC_HEREIS is the exception, because the buffers haven't been allocated yet.)
 */

    bchdr_p bc;
    char *data;
    f_size_t n;
    int bigendian;
    long *protocol;

    if(ADR_NULL(dst)) {
	PROTO_LOOK_HEADER(protocol, pkt, long);
	if(protocol != 0)  {
	    dec_l_be(protocol);	/* protocol field is sent in network order */
	}
	if(protocol == 0 || *protocol != FLIP_AMGROUP || offset != 0) {	
	    pkt_discard(pkt);
	    return;
	}
	proto_remove_header(pkt);
	total -= sizeof(long);
	length -= sizeof(long);
    }
    if(offset == 0)  {	/* first fragment */
	if(length == total) {	/* message */
	    PROTO_LOOK_HEADER(bc, pkt, bchdr_t);
	    if(bc == 0) {
		printf("grp_arrived: packet without header\n");
		pkt_discard(pkt);
		return;
	    }
	    bigendian = bc->b_flags & F_BIGENDIAN;
	    bc_orderhdr(bc, bigendian);
	    proto_remove_header(pkt);
#ifdef MEASURE
	    if (bc->b_type == BC_BCAST) BEGIN_MEASURE(grp_member);
	    if (bc->b_type == BC_BCASTREQ) BEGIN_MEASURE(grp_sequencer);
#endif
	    if(bc->b_type != BC_HEREIS) {
		if (!split(g, pkt, &data, &n)) { /* wrong size? */
		    pkt_discard(pkt);
		    return;
		}
	    } else {		/* no buffers yet to store msg */
		data = pkt_offset(pkt);
		n = pkt->p_contents.pc_totsize;
	    }
	    grp_switch(g, src, bc, data, n, bigendian);
	    END_MEASURE(grp_member);
	} else  {	/* first fragment */
	    grp_reassemble(g, src, messid, length, total, pkt);
	}
    } else {		/* a not-first fragment arrived */
	grp_data(g, src, messid, offset, length, total, pkt);
    }
    pkt_discard(pkt);
}


/*ARGSUSED*/
static void grp_notdeliver(g, pkt, dst, messid, offset, length, total, flags)
    group_p g;
    pkt_p pkt;
    adr_p dst;
    f_msgcnt_t messid;
    f_size_t offset, length, total;
    int flags;
{
/* The FLIP layer could not deliver the packet. */

    int dontwakeup = 0;
    member_p m;

    GDEBUG(LM, printf("grp_notdeliver (%d): ", group_no(g));
	       badr_print((char *) 0, (char *) 0, dst); printf("\n"));
    GRP_FIND_MEMBER(g, dst, m);
    if (m != 0 && IS_MEMBER(m->m_state)) {
	if (flags == 0) {
	    GDEBUG(LM, printf("grp_notdeliver: consider %d dead\n",
			      mem_no(g, m)));
	    m->m_retrial = -1;
	}
    }
    if (pkt->p_admin.pa_release == settimer ||
	pkt->p_admin.pa_release == setresettimer) {
	pkt->p_admin.pa_release = 0; /* don't set timer */
    } else if (pkt->p_admin.pa_release == setresettimer_multi) {
	/* We sent away multiple packets, and we don't want to wakeup the
	 * sender just because one packet couldn't be delivered.
	 * In this case we *do* allow pkt_discard to set the reset timer,
	 * so that eventually the sender will be awoken when not enough
	 * reponses arrive for the other packets.
	 */
	dontwakeup = 1;
    }
    pkt_discard(pkt);

    /* Wakeup the sender of the packet, as sending failed. */
    if(g->g_flags & FL_SENDING) {
	wakeup((event) &g->g_sender);
    }
    if(g->g_flags & FL_RESET) {
	if (dontwakeup) {
	    GDEBUG(LM, printf("grpnotdeliver: don't wakeup(reset)\n"));
	} else {
	    GDEBUG(LM, printf("grpnotdeliver: wakeup(reset)\n"));
	    wakeup((event) &g->g_reset);
	}
    }
}


static void grp_sweep(id)
    long id;
{
/* Sweep through the alive, sync, and reply timers. */

    register group_p g;
    
    for(g = grouptab; g < grouptab + grp_maxgrp; g++) {
	if(!g->g_used) continue;
	if(g->g_replytimer > 0) {
	    g->g_replytimer -= id;
	    if(g->g_replytimer <= 0) {
#ifndef NO_CONGESTION_CONTROL
		/* Because of the low sweeper resolution of 100 msec,
		 * we must check if there really is a timeout on the
		 * current msg.
		 */
		uint32 now = getmilli();

		if (g->g_cng_start_send + g->g_cng_retrans > now) {
		    GDEBUG(LM, printf("grp_sweep: no timeout (%d+%d > %d)\n",
				  g->g_cng_start_send, g->g_cng_retrans, now));
		    g->g_replytimer =
			        (g->g_cng_start_send + g->g_cng_retrans) - now;
		} else
#endif
		{
		    STINC(st_replytimeout);
		    GDEBUG(LF, printf("grp_sweep: wakeup sender\n"));
		    g->g_replytimer = -1;
		    wakeup((event) &g->g_sender);
		}
	    }
	}
	if(g->g_me == g->g_seq && g->g_synctimer > 0) {
	    g->g_synctimer -= id;
	    if(g->g_synctimer <= 0) {
		STINC(st_synctimeout);
		synchronize(g, 1);
		setsynctimer(g);
	    }
	}
	if(g->g_alivetimer > 0) {
	    g->g_alivetimer -= id;
	    if(g->g_alivetimer <= 0) {
		checkliveness(g);
		setalivetimer(g);
	    }
	}
	if(g->g_flags & FL_RESET && g->g_flags & FL_RECEIVING) {
	    wakeup((event) &g->g_receiver);
	}
	if(g->g_resettimer > 0) {
	    g->g_resettimer -=id;
	    if(g->g_resettimer <= 0 && g->g_flags & FL_RESET) {
		g->g_resettimer = -1;
		GDEBUG(LM, printf("grpsweep: wakeup(reset)\n"));
		wakeup((event) &g->g_reset);	
	    }
	}
#ifndef NO_CONGESTION_CONTROL
	/* The congestion timer is currently only used to maintain
	 * the min and max roundtrip values of the last 5 seconds.
	 * They are not used in the congestion algorithm itself.
	 */
	if (g->g_cng_timer > 0) {
	    g->g_cng_timer -= id;
	    if (g->g_cng_timer <= 0) {
		g->g_cng_timer = g->g_cng_timeout;
		g->g_cng_min_roundtrip = 0;
		g->g_cng_max_roundtrip = 0;
	    }
	}
#endif
    }
}


/*
 * 	Functions called from other parts in the kernel.
 */


void grp_destroy(t)
    struct thread *t;
{
/* Destroy the group that was created or joined by process pid. If this
 * group entry belongs to the sequencer and the member has left the group
 * (using grp_leave), keep the group entry for some time to give the other
 * members the oppurtunity to continue.
 */

    group_p g;
    pkt_p msg;
    
    for(g=grouptab; g < grouptab + grp_maxgrp; g++) {
	if(!g->g_used) continue;
	if(g->g_pid == getpid(t)) {
	    /* if the DESTROY flag has been set, g->g_me->m_ep has become
	     * unregistered, so we cannot multicast anymore.
	     * TODO: what about the condition "g->g_total <= 0"?
	     */
	    if(g->g_me == g->g_seq && !(g->g_flags & FL_DESTROY)) {
		if(g->g_total_noseq > 0) {
		    /* Inform other members that we suddenly died. */
		    if((g->g_flags & FL_SIGNAL) &&
		       fillmsg(g, &msg, (header_p) 0, BC_DEAD, 0L, 0L,
			       g->g_index, (char *) 0, 0L)) {
			STINC(st_sdead);
			do_flip_multicast(g->g_ifno, msg, 0, &g->g_addr,
				     g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
				     g->g_total_noseq, g->g_ltimeout,
				     "grp_destroy1");
		    }
		}
		deletegroup(g);
	    } else if(g->g_me == g->g_seq) {
		g->g_flags |= FL_NOUSER;
	    } else if(g->g_me != g->g_seq) {
		/* The group structure does not belong to the sequencer. So,
		 * always free it. If the member didn't perform a grp_leave,
		 * send a dead msg to the other group members.
		 */
		if(!(g->g_flags & FL_DESTROY)) {
		    /* Inform other members that we suddenly died. */
		    if((g->g_flags & FL_SIGNAL) &&
		       (g->g_total > 1) &&
		       fillmsg(g, &msg, (header_p) 0, BC_DEAD, 0L, 0L,
			       g->g_index, (char *) 0, 0L)) {
			STINC(st_sdead);
			do_flip_multicast(g->g_ifno, msg, 0, &g->g_addr_all, 
					  g->g_me->m_ep,
					  (f_size_t) sizeof(bchdr_t),
					  g->g_total-1, g->g_ltimeout,
					  "grp_destroy2");
		    }
		}
		deletegroup(g);
	    }
	}
    }
}

#ifndef NO_MPX_REGISTER
static void
grp_exit(t)
    struct thread *t;
{
    /* if last thread of the process is leaving check if has group
     * status that has to be deleted.
     */
    if (mpx_nthread(t) == 1) {
	grp_destroy(t);
    }
}
#endif

int grp_sendsig(t)
    struct thread *t;
{
/* A thread has received a signal; if the thread belongs to a member process,
 * take the appropriate action.
 */

    event mpx_threadevent();
    group_p g, lastgroup;
    int r;
    
    if (ngroups_in_use == 0) {
	/* quick exit in case no group is currently used */
	return(0);
    }

    r = 0;
    lastgroup = grouptab + grp_maxgrp;
    for(g=grouptab; g < lastgroup; g++) {
	if(!g->g_used) continue;
	if(g->g_pid == getpid(t)) {
	    if ((g->g_flags & FL_SIGNAL) == 0) {
		GDEBUG(LM, printf("grp_sendsig(%d): group %d signalled\n",
				  THREADSLOT(t), group_no(g)));
		g->g_flags |= FL_SIGNAL;
	    }
	    if ((g->g_flags & FL_SENDING) &&
		(mpx_threadevent(t) == (event) &g->g_sender))
	    {
		GDEBUG(LM, printf("grp_sendsig(%d): signal during send\n",
				  THREADSLOT(t)));
		r = 1;
	    }
	}
    }
    return(r);
}


void grp_stopgrp(t)
    struct thread *t;
{
/* A thread is forced to terminate. It will never run again. */
    
    group_p g;
    event mpx_threadevent();
    
    for(g=grouptab; g < grouptab + grp_maxgrp; g++) {
	if(!g->g_used) continue;
	if(g->g_pid == getpid(t)) {
	    GDEBUG(LM, printf("grp_stopgrp: grp %d, thread %d, event 0x%lx: ",
			      group_no(g), THREADSLOT(t), mpx_threadevent(t)));
	    if(mpx_threadevent(t) == (event) &g->g_sender) {
		/* thread is blocked in send. */
		GDEBUG(LM, printf("sending\n"));
		g->g_flags &= ~FL_SENDING;
		wakeup((event) &g->g_sender);
		if(g->g_fbarrier != 0) unblock(g);
		else g->g_flags &= ~FL_QUEUE;
	    } else if(mpx_threadevent(t) == (event) &g->g_receiver) {
		/* thread is blocked in receive. */
		GDEBUG(LM, printf("receiving\n"));
		wakeup((event) &g->g_receiver);
	    } else if(mpx_threadevent(t) == (event) &g->g_reset) {
		/* thread is blocked in reset. */
		GDEBUG(LM, printf("resetting\n"));
		wakeup((event) &g->g_reset);
	    } else {
		GDEBUG(LM, printf("queued in send?\n"));
		delblock(g, mpx_threadevent(t));
	    }
	}
    }
}

static errstat
grp_find(gd, p, gp)
    g_id_t gd;
    port *p;
    group_p *gp;
{
    register group_p g;

    if (gd < 0 || gd >= (int) grp_maxgrp) {
	return(STD_ARGBAD);
    }
    g = groupindex[gd];
    if (!PORTCMP(&g->g_port, p)) {
	return(BC_BADPORT);
    }
    if (g->g_pid != getpid(curthread)) {
	return(STD_ARGBAD);
    }

    *gp = g;
    return STD_OK;
}

int grp_memaddress(p, gd, memid, memaddr)
    port *p;
    g_id_t gd;
    g_indx_t memid;
    adr_p memaddr;
{
/* This routine is called from the RPC module; it is used to implement
 * rpc_forward.
 */

    group_p g;
    errstat err;

    if ((err = grp_find(gd, p, &g)) != STD_OK) {
	return err;
    }

    if(!IS_MEMBER(g->g_member[memid].m_state)) return(STD_ARGBAD);
    *memaddr = g->g_member[memid].m_rpcaddr;
    return 1;
}


void
grp_init()
{
/* Initialize the group communication module. */

    group_p g;
    barrier_p bp, barrierlist;
    int i;
    
    grouptab = (group_p) aalloc((vir_bytes)(grp_maxgrp * sizeof(group_t)), 0);
    groupindex = (group_p *)
			aalloc((vir_bytes)(grp_maxgrp * sizeof(group_p)), 0);
    for(g = grouptab, i = 0; g < grouptab+grp_maxgrp; g++, i++) {
	groupindex[i] = g;
	g->g_used = 0;
    }
    DPRINTF(0,("grp_init: %d group(s) of %d allocated\n", grp_maxgrp,
	       sizeof(group_t)));
    DPRINTF(0,("grp_init: aalloc %d group pkts(%d)\n", grp_npkt, grp_pktsize));
    grp_pkt = (pkt_p) aalloc((vir_bytes)(sizeof(pkt_t) * grp_npkt), 0);
    grp_pktdata = aalloc((vir_bytes)(grp_npkt * grp_pktsize), 0);
    pkt_init(&grp_pool, (int) grp_pktsize, grp_pkt, (int) grp_npkt,
					    grp_pktdata, (void (*)()) 0, 0L);
    sweeper_set(grp_sweep, 500L, 500L, 0);
    grp_debug_level = GRP_DEBUG_LEVEL;
    barrierlist = (barrier_p)
		    aalloc((vir_bytes)(grp_nbarrier * sizeof(barrier_t)), 0);
    freebarrier = 0;
    for(bp = barrierlist; bp < barrierlist + grp_nbarrier-1; bp++) {
	bp->b_next = freebarrier;
	freebarrier = bp;
    }
    if(sizeof(bchdr_t) != 20) panic("grp_init: size bc header is not 20\n");

#ifndef NO_MPX_REGISTER
    mpx_register((void (*)()) 0, grp_stopgrp, grp_sendsig, grp_exit);
#endif
}


/*
 * 	User calls for group communication.
 */

static errstat grp_set_param(g, groupvar, value)
    group_p g;
    int     groupvar;
    long    value;
{
    /* New, more flexible grp_set interface: only set the single group
     * variable specified.
     */

    /* Allow only non-negative values.
     * Some more sanity checking could be appropriate, depending on
     * the group variable involved.
     */
    if (value < 0) {
	return STD_ARGBAD;
    }

    switch (groupvar) {
    case GRP_VAR_SYNC_TIMEOUT: 
	g->g_stimeout = value;
	if (g->g_synctimer > value) { /* reset current timer as well */
	    g->g_synctimer = value;
	}
	break;
    case GRP_VAR_REPLY_TIMEOUT:
	g->g_rtimeout = value;
	if (g->g_replytimer > value) { /* reset current timer as well */
	    g->g_replytimer = value;
	}
	break;
    case GRP_VAR_ALIVE_TIMEOUT:
	g->g_atimeout = value;
	if (g->g_alivetimer > value) { /* reset current timer as well */
	    g->g_alivetimer = value;
	}
	break;
    case GRP_VAR_RESET_TIMEOUT:
	g->g_resettimeout = value;
	if (g->g_resettimer > value) { /* reset current timer as well */
	    g->g_resettimer = value;
	}
	break;
    case GRP_VAR_LOCATE_TIMEOUT:
	g->g_ltimeout = value;
	break;
    case GRP_VAR_MAX_RETRIAL:
	g->g_maxretrial = value;
	break;
    case GRP_VAR_DEBUG_LEVEL:
	grp_debug_level = value;
	break;
    default:
	return STD_ARGBAD;
    }

    return STD_OK;
}

errstat grp_set(gd, p, stimeout, rtimeout, atimeout, large)
    g_id_t gd;
    port *p;
    interval stimeout, rtimeout, atimeout;
    f_size_t large;
{
/* Set timeouts, the debug level, and when method PB or method BB
 * should be used.
 */

    group_p g;
    errstat err;

    if ((err = grp_find(gd, p, &g)) != STD_OK) {
	return err;
    }

    if (stimeout < 0) {
	/* use new interface; should replace old interface eventually */
	return grp_set_param(g, (int) rtimeout, (long) atimeout);
    }

    g->g_large = large;
    if ((err = grp_set_param(g, GRP_VAR_SYNC_TIMEOUT, stimeout)) != STD_OK) {
	return err;
    }
    if ((err = grp_set_param(g, GRP_VAR_REPLY_TIMEOUT, rtimeout)) != STD_OK) {
	return err;
    }
    if (atimeout < 1000) {
	/* hack; will soon be removed */
	err = grp_set_param(g, GRP_VAR_DEBUG_LEVEL, atimeout);
    } else {
	err = grp_set_param(g, GRP_VAR_ALIVE_TIMEOUT, atimeout);
    }

    return err;
}


errstat grp_info(gd, p, state, memlist, size)
    g_id_t gd;
    port *p;
    grpstate_p state;
    g_indx_t memlist[];
    g_indx_t size;
{
/* Return information about the group to the application. */

    group_p g;
    g_indx_t i, j = 0;
    hist_p hist;
    errstat err;
    
    if ((err = grp_find(gd, p, &g)) != STD_OK) {
	return err;
    }
    /* Check that the other arguments are ok */
    if (size > 0 && memlist == 0) {
	return STD_ARGBAD;
    }

    state->st_expect = g->g_me->m_expect;
    state->st_nextseqno = g->g_nextseqno;
    if(g->g_resilience > 0) {
	for(state->st_unstable = g->g_nextseqno; 1 ; state->st_unstable++) {
	    hist = &g->g_history[HST_MOD(g, state->st_unstable)];
	    if(hist->h_bc.b_seqno < state->st_unstable)
		break;
	    compare(hist->h_bc.b_seqno, ==, state->st_unstable);
	}
    } else state->st_unstable = g->g_nextseqno;
    state->st_total = g->g_total;
    state->st_myid = g->g_index;
    state->st_seqid = g->g_seqid;
    for(i=0; i < g->g_maxgroup; i++)
	if(IS_LIVINGMEMBER(g->g_member[i].m_state)) 
	    if(j < size) memlist[j++] = i;
    return STD_OK;
}


g_id_t grp_create(p, resilience, maxgroup, nbuf, maxmess)
    port *p;
    g_indx_t resilience;
    g_indx_t maxgroup;
    uint32 nbuf;		/* 2log */
    uint32 maxmess;		/* 2log */
{
/* Create a group. */

    group_p g;
    adr_t adr;
    struct thread *t = curthread;
    
    if(NULLPORT(p)) return(BC_BADPORT);
    if(group_exist(getpid(t), p)) return(STD_EXISTS);
    if(maxgroup <= 1 || nbuf < MINNBUF) return(STD_ARGBAD);
    if(maxmess < MINLOGMSG) return(STD_ARGBAD);
    if(resilience >= MAXRESILIENCE) return(STD_ARGBAD);
    if((g = getgroup(p)) == 0) return(BC_TOOMANY);
    g->g_maxgroup = (g_index_t) maxgroup;
    g->g_nloghist = (uint16) nbuf;
    g->g_logsizemess = (uint16) maxmess;
    g->g_resilience = (g_index_t) resilience;
    g->g_seqid = 0;
    g->g_index = 0;
    if(!initgroup(g)) {
	freegroup(g);
	return(STD_NOMEM);
    }
    g->g_seq = &g->g_member[g->g_seqid];
    g->g_me = g->g_seq;
    g->g_total = 1;
    g->g_total_noseq = 0;
    g->g_me->m_state = M_MEMBER;
    adr_random(&g->g_prvaddr);
    flip_oneway(&g->g_prvaddr, &g->g_addr);
    adr_random(&g->g_prvaddr_all);
    flip_oneway(&g->g_prvaddr_all, &g->g_addr_all);
    adr_random(&adr);
    setsynctimer(g);
    setalivetimer(g);
    if((g->g_bcastep = flip_register(g->g_ifno, &nulladdr)) < 0) 
	panic("grp_create: flip_register bcast failed");
    if((g->g_me->m_ep = flip_register(g->g_ifno, &adr)) < 0) 
	panic("grp_create: flip_register failed");
    if((g->g_ep_all = flip_register(g->g_ifno, &g->g_prvaddr_all)) < 0) 
	panic("grp_create: flip_register failed");
    flip_oneway(&adr, &g->g_member[0].m_addr);    
    rpc_myaddr(&g->g_member[g->g_index].m_rpcaddr);
    return(group_no(g));
}


static void delblock(g, ev)
    group_p g;
    event ev;
{
/* The thread blocked on ev, is forced to stop; remove it from the list and
 * wake it up.
 */

    barrier_p bp, prev;

    for(bp = g->g_fbarrier, prev = 0; bp != 0; bp = bp->b_next) {
	if((event) &bp->b_barrier == ev) {
	    DPRINTF(0, ("delblock: remove entry from blocked list\n"));
	    if(prev == 0) g->g_fbarrier = bp->b_next;  /* first in the list? */
	    else prev->b_next = bp->b_next;
	    if(g->g_fbarrier == 0) {		       /* empy list? */
		g->g_lbarrier = &g->g_fbarrier;
	    } else if(&bp->b_next == g->g_lbarrier) {  /* last entry on list */
		assert(prev != 0);
		g->g_lbarrier = &prev->b_next;
	    }
	    bp->b_next = freebarrier;		       /* put in free list */
	    freebarrier = bp;
	    wakeup((event) &bp->b_barrier);
	    return;
	} else prev = bp;
    }
}


static int block(g)
    group_p g;
{
/* Block the sender until all other sends are finished. Called by dosend(). */
    
    barrier_p b;
    
    DPRINTF(0, ("block: block sender\n"));
    b = freebarrier;
    if(b == 0) {
	DPRINTF(0, ("block: out of barriers\n"));
	return(BC_TOOMANY);
    }
    freebarrier = b->b_next;
    b->b_next = 0;
    *g->g_lbarrier = b;
    g->g_lbarrier = &b->b_next;
    if(await_reason((event) &b->b_barrier, 0L, "dosend_blocked") < 0) {
	DPRINTF(0, ("block: await failed\n"));
	delblock(g, (event) &b->b_barrier);
	return(BC_FAIL);
    }
    if(!g->g_used) return(STD_ARGBAD);

    assert(g->g_fbarrier == b);
    if((g->g_fbarrier = b->b_next) == 0) {	/* last barrier? */
	g->g_lbarrier = &g->g_fbarrier;
    }
    b->b_next = freebarrier;
    freebarrier = b;
    return STD_OK;
}


static void unblock(g)
    group_p g;
{
/* Dosend() is finished. See if somebody is blocked. */
    
    barrier_p b;
    
    DPRINTF(0, ("unblock: unblock sender\n"));
    assert(g->g_fbarrier);
    b = g->g_fbarrier;
    wakeup((event) &b->b_barrier);
}


static errstat dosend(gd, hdr, data, size, type)
    g_id_t gd;
    header_p hdr;
    bufptr data;
    f_size_t size;
    uint8 type;
{
/* Dosend is called by grp_join(), grp_send(), and grp_leave(). It sends a
 * message reliable and globally ordered to the group.
 * A process may only have one outstanding dosend(). If another thread of the
 * process is busy sending, the calling thread is blocked until the outstanding
 * threads are finished.
 * Dosend() has two ways of sending a message to the group: method PB or method
 * BB. Depending on size and g_large, one of them is chosen.
 * If while sending one of the members fails and the group enters the recovery
 * state, the send is blocked until it is known if the send has succeeded or
 * not.
 */

    group_p g_var;
    register group_p g;
    g_msgcnt_t messid;
    f_size_t l;
    pkt_p msg;
    int f = 0;
    errstat err;
#ifndef NO_CONGESTION_CONTROL
    unsigned long now;
    long roundtrip; /* must be signed because of computations below */
    f_size_t size_kb;
    interval retrans_time;
#endif
    
    BEGIN_MEASURE(grp_snd);
    BEGIN_MEASURE(grp_snd_snd);
    
    if ((err = grp_find(gd, &hdr->h_port, &g_var)) != STD_OK) {
	return err;
    }
    g = g_var;

    /* Sanity checks. */
    if(size > g->g_maxsizemess - sizeof(header)) return(BC_TOOBIG);
    if(g->g_flags & FL_QUEUE) {		/* a thread busy sending? */
	int r;

	if((r = block(g)) != STD_OK) return(r);		/* block */
    }
    if(g->g_flags & FL_DESTROY) return(BC_NOEXIST);
    if(g->g_flags & FL_RESET) return(BC_ABORT);
    if(g->g_flags & FL_NOUSER) return(BC_NOEXIST);
    g->g_flags &= ~FL_SIGNAL;
    
    if(size > 0 && data == 0)  
	return(STD_ARGBAD);
    assert(g->g_used);
    assert(!(g->g_flags & FL_SENDING));
    g->g_flags |= FL_QUEUE;
    g->g_seq->m_retrial = g->g_maxretrial;
    messid = g->g_me->m_messid+1;
    g->g_flags |= FL_SENDING;
#ifndef NO_CONGESTION_CONTROL
    now = g->g_cng_start_send = getmilli();
    /* Small messages are counted as 1Kb */
    size_kb = (size < 1024) ? 1 : (size / 1024);
    /* save current retransmission timer value */
    retrans_time = g->g_cng_retrans;
#endif
    do {
	if(!(g->g_flags & FL_RESET)) {
#ifndef NO_CONGESTION_CONTROL
	    /* Because of the low sweeper time resolution, we currently
	     * implement the data rate restriction in a rather simpleminded
	     * manner.
	     * Given the amount of data we already have transmitted during the
	     * current time slice, we determine whether we can put the new
	     * message on the network right now, or whether we have to
	     * wait for a later timeslice.
	     * TODO: in some timeslice we might transmit more than the
	     * allowed data rate, because the message happened to be that big.
	     * Maybe we should cater for that by disallowing transmission
	     * during the next few timeslices.  For now we ignore this issue.
	     */
	    if (now >= g->g_cng_start_slice + CNG_SLICE) {
		/* Start new time slice. */
		g->g_cng_start_slice = now;
		g->g_cng_sent = 0;
	    } else {
		if (g->g_cng_sent + size_kb > g->g_cng_cur_rate) {
		    /* Already sent the allowed data in this timeslice.
		     * Wait for the next.  Note that the current mpx sweeper
		     * will let us wait any time between 0 and 100 (CNG_SLICE)
		     * millisec, but that might change.
		     */
		    interval waittime = g->g_cng_start_slice + CNG_SLICE - now;

		    compare(waittime, >, 0);
		    compare(waittime, <=, 100);
		    GDEBUG(LM, printf("gsend %ld sent %ld rate %ld wait %ld\n",
				      size_kb, g->g_cng_sent,
				      g->g_cng_cur_rate, waittime));
		    (void) await((event) 0, waittime);
		    g->g_cng_start_slice = getmilli();
		    g->g_cng_sent = 0;
		    GDEBUG(LM, printf("grp_send: waited %ld msec\n",
				      g->g_cng_start_slice - now));
		    if (now == g->g_cng_start_send) {
			/* don't include the time we waited in the roundtrip */
			g->g_cng_start_send = g->g_cng_start_slice;
		    }
		}
	    }
	    g->g_cng_sent += size_kb;
	    g->g_cng_tot_sent += size_kb;
#endif
	    if(fillmsg(g, &msg, hdr, type, 0L, messid, g->g_index, data, size))
	    { 
		SETTIMER(g, msg, settimer);
		STINC(st_sbcastreq);
		/* Wait for g_maxsilent broadcast messages to arrive
		 * before sending seqno.
		 */
		g->g_silent = WAIT_SILENT(g);
		l = sizeof(header) + sizeof(bchdr_t) + size;
		END_MEASURE(grp_snd_snd);
		if(g->g_seq == g->g_me) { /* sequencer? */
		    grp_arrived(g, msg, &g->g_seq->m_addr, &g->g_seq->m_addr,
				0L, 0L, l, l); 

		} else if(size + sizeof(header) >= g->g_large) {/* method BB*/ 
		    do_flip_multicast(g->g_ifno, msg, f, &g->g_addr_all,
				      g->g_me->m_ep, l, g->g_total,
				      g->g_ltimeout, "dosend");
		} else {	/* method PB */
		    do_flip_unicast(g->g_ifno, msg, f, &g->g_seq->m_addr, 
				    g->g_me->m_ep, l, g->g_ltimeout, "dosend");
		}
	    } else {
		/* out of packets; try again later */
		settimer(group_no(g));
	    }
	} else if(g->g_flags & FL_JOINING) {
	    /* If we are joining, we have to return an error now, because
	     * there is no thread that can start the recovery. Furthermore,
	     * we did not receive any messages yet, so we can just quit.
	     */
	    g->g_flags &= ~FL_SENDING;
	    g->g_flags |= FL_FAILED;
	    /* Still have to clear FL_QUEUE or unblock waiting member */
	    break;
	}
	f = 0;
	if(g->g_flags & FL_SENDING) {
	    if ((await_reason((event) &g->g_sender, 0L, "grp_send") < 0) ||
		(g->g_flags & FL_SIGNAL))
	    {
		GDEBUG(LR, printf("grp_send: signalled\n"));
	        g->g_flags &= ~FL_SENDING;
		g->g_flags |= FL_FAILED;
		/* Still have to clear FL_QUEUE or unblock waiting member */
		break;
	    }
	}
	BEGIN_MEASURE(grp_snd_rcv);
	if(g->g_flags & FL_SENDING && g->g_index != g->g_seqid) {
	    g->g_seq->m_retrial--;
	    if(g->g_seq->m_retrial <= FIRSTLOCATE) f |= FLIP_INVAL;
	    if(g->g_seq->m_retrial <= 0) {
		GDEBUG(LM, printf("grp_send(%d): no more retries\n",
				  group_no(g))); 
		switchstate(g);
	    } else {
		GDEBUG(LM, printf("grp_send(%d): timeout for mid %d; %d retries\n",
				  group_no(g), messid, g->g_seq->m_retrial));
		STINC(st_stimedout);
#ifndef NO_CONGESTION_CONTROL
		/* Timeout indicates possible congestion.  We'll try to find
		 * a new equilibrium by means of the slow-start algorithm.
		 */
		g->g_cng_thresh_rate = g->g_cng_cur_rate / 2;
		g->g_cng_cur_rate = CNG_INIT_RATE;

		/* Also increase the retransmission timer temporarily,
		 * since congestion may have built up suddenly.  A new
		 * retrans timer value will be computed below.
		 */
		if (g->g_cng_retrans < 2000) {
		    g->g_cng_retrans *= 2;
		    GDEBUG(LM, printf("grp_send: retrans time now %ld\n",
				      g->g_cng_retrans));
		}
#endif
	    }
	}
#ifndef NO_CONGESTION_CONTROL
	now = getmilli();
#endif
    } while(g->g_flags & FL_SENDING);

    if(g->g_flags & FL_FAILED) {
	g->g_flags &= ~FL_FAILED;
	err = BC_FAIL;
    } else {
#ifndef NO_CONGESTION_CONTROL
	roundtrip = getmilli() - g->g_cng_start_send;
	if (roundtrip < 0) {
	    /* timer wrap? */
	    roundtrip = retrans_time;
	}

	/* Because of the low timer resolution, we might have received
	 * the acknowledgement later then we expected, but still we didn't
	 * get a timeout.  In that case, we update the slow-start congestion
	 * rates here.
	 */
	if (roundtrip > retrans_time && g->g_cng_cur_rate != CNG_INIT_RATE) {
	    g->g_cng_thresh_rate = g->g_cng_cur_rate / 2;
	    g->g_cng_cur_rate = CNG_INIT_RATE; /* Kb/slice */
	    GDEBUG(LM, printf("grp_send: roundtrip %ld > %ld: slowstart %ld\n",
			      roundtrip, retrans_time, g->g_cng_thresh_rate));
	}

	if (g->g_cng_min_roundtrip == 0 || roundtrip < g->g_cng_min_roundtrip){
	    g->g_cng_min_roundtrip = roundtrip;
	}
	if (roundtrip > g->g_cng_max_roundtrip) {
	    g->g_cng_max_roundtrip = roundtrip;
	}

	/* update statistics using Van Jacobsen's SIGCOMM'88 algorithm: */
	roundtrip -= (g->g_cng_avg_roundtrip >> 3);
	g->g_cng_avg_roundtrip += roundtrip;
	if (roundtrip < 0) {
	    roundtrip = -roundtrip;
	}
	g->g_cng_sdv_roundtrip += (roundtrip - (g->g_cng_sdv_roundtrip >> 2));
	g->g_cng_retrans =
		((g->g_cng_avg_roundtrip >> 2) + g->g_cng_sdv_roundtrip) >> 1;

	/* update allowed data rate */
	if (g->g_cng_cur_rate < g->g_cng_thresh_rate) {
	    /* "slow-start" actually increases the data rate exponentially: */
	    g->g_cng_cur_rate *= 2;
	} else {
	    /* past the threshold we increase the rate more carefully: */
	    g->g_cng_cur_rate += 1;
	}
	/* As a sanity check, we currently don't set it higher than what
	 * 10Mb/sec ethernet can possibly handle.
	 */
	if (g->g_cng_cur_rate > CNG_MAX_RATE) {
	    g->g_cng_cur_rate = CNG_MAX_RATE;
	}
#endif
	err = STD_OK;
    }
    if(g->g_fbarrier != 0) unblock(g);
    else g->g_flags &= ~FL_QUEUE;
    END_MEASURE(grp_snd_rcv);
    END_MEASURE(grp_snd);
    return err;
}


g_id_t grp_join(hdr)
    header_p hdr;
{
/* Make the caller member of the group specified by the port in the hdr. Joining
 * works in two phases. In the first phase (FL_LOCATING), the sequencer of the
 * group is located. In the reply of the locate info is stored that is needed
 * to allocate the datastructures for the locating member, so that in the
 * second phase the same code as for grp_leave and grp_send can be used.
 * In the second phase, the joining member tries to become a member. It
 * does this by calling dosend(). The only difference between grp_leave() and
 * grp_send(), and the call of grp_join to sosend(), that the flag FL_JOINING
 * is set while trying to join. Using the flag grp_switch can discard any
 * message other than a BC_JOIN.
 */

    group_p g;
    int ep;
    f_hopcnt_t hops;
    pkt_p msg;
    adr_t adr, pubaddr, rpcaddr;
    long *protocol;
    struct thread *t = curthread;
    errstat res;
    interval ltime = 100L;
    int retrial = BCRETRIAL;
    
    if(NULLPORT(&hdr->h_port)) return(BC_BADPORT);
    if(group_exist(getpid(t), &hdr->h_port)) return(STD_EXISTS);
    if((g = getgroup(&hdr->h_port)) == 0) return(BC_TOOMANY);

    /* Register an address at the FLIP interface and start first phase. */
    adr_random(&adr);
    g->g_flags |= FL_LOCATING;

    if((ep = flip_register(g->g_ifno, &adr)) < 0)
	panic("grp_join: flip_register failed\n"); 
    flip_oneway(&adr, &pubaddr);
    rpc_myaddr(&rpcaddr);

    /* Find the sequencer for this group. */
    hops = 0;
    do {
	g->g_flags |= FL_LOCATING;
	if(fillmsg(g, &msg, hdr, BC_LOCATE, 0L, 0L, 0, (char *) 0, 0L)) {
	    STINC(st_slocate);
	    PROTO_ADD_HEADER(protocol, msg, long);
	    *protocol = FLIP_AMGROUP;
	    enc_l_be(protocol);	/* send protocol field in network order. */
	    proto_fix_header(msg);
	    proto_dir_append(msg, &rpcaddr, sizeof(adr_t));
	    proto_dir_append(msg, &pubaddr, sizeof(adr_t));
	    flip_broadcast(g->g_ifno, msg, ep, (f_size_t)
			   sizeof(bchdr_t) + sizeof(header) + sizeof(long) +
			   2 * sizeof(adr_t), hops); 
	}
	if(g->g_flags & FL_LOCATING) {
	    (void) await_reason((event) &g->g_sender, ltime, "grp_join");
	}

	if(hops >= max_hops) ltime = ltime * 2;
	else hops++;

	if(g->g_flags & FL_LOCATING && hops >= max_hops && --retrial < 0) {
	    printf("could not find the sequencer\n");
	    if (flip_unregister(g->g_ifno, ep) < 0)
		panic("grp_join: couldn't unregister ep (b)");
	    freegroup(g);
	    return(BC_NOEXIST);
	}
    } while(g->g_flags & FL_LOCATING);
    if(!initgroup(g)) {	/* see if we can allocate all the datastructures */
	if (flip_unregister(g->g_ifno, ep) < 0)
	    panic("grp_join: couldn't unregister ep (b)");
	freegroup(g);
	return(STD_NOMEM);
    }

    /* The sequencer is found. Initialize datastructures. */

    g->g_me = &g->g_member[g->g_index];
    g->g_me->m_addr = pubaddr;
    g->g_me->m_ep = ep;
    g->g_seq = &g->g_member[g->g_seqid];
    g->g_seq->m_addr = g->g_addr_all; 	/* have used it temporarily */
    rpc_myaddr(&g->g_me->m_rpcaddr);
    g->g_total = 2;			/* the sequencer and me */
    g->g_total_noseq = 1;
    if((g->g_ep = flip_register(g->g_ifno, &g->g_prvaddr)) < 0)
	panic("grp_join: flip_register failed\n");
    if((g->g_ep_all = flip_register(g->g_ifno, &g->g_prvaddr_all)) < 0)
	panic("grp_join: flip_register failed");
    flip_oneway(&g->g_prvaddr_all, &g->g_addr_all);
    g->g_me->m_state = M_JOINING;
    g->g_seq->m_state = M_JOINING;
    g->g_me->m_messid = 0;

    /* Second phase: try to become a member. */
    res = dosend((g_id_t) group_no(g), hdr, (bufptr) 0, (f_size_t) 0,
		 BC_JOINREQ);
    if(ERR_STATUS(res)) {
	printf("grp_join: failed %d\n", res);
	deletegroup(g);
	return(BC_FAIL);
    }
    g->g_seq->m_retrial = g->g_maxretrial;
    setalivetimer(g);
    return(group_no(g));
}



errstat grp_send(gd, hdr, data, size)
    g_id_t gd;
    header_p hdr;
    bufptr data;
    f_size_t size;
{
    return(dosend(gd, hdr, data, size, BC_BCASTREQ));
}


errstat grp_leave(gd, hdr)
    g_id_t gd;
    header_p hdr;
{
    return(dosend(gd, hdr, (bufptr) 0, 0L, BC_LEAVEREQ));
}


int32 grp_receive(gd, hdr, buf, cnt, more)
    g_id_t gd;
    header_p hdr;
    bufptr buf;
    f_size_t cnt;
    int *more;
{
/* Receive a message from the group. If no message is available, block.
 * If one of the members fails or if the process is signalled, grp_receive
 * returns BC_ABORT to indicate that grp_reset() can be called.
 * After delivering my own leave, the group entry must be removed, because
 * the member is not allowed to send or receive message from the group.
 * The exception to this rule is the sequencer; we will note that there is
 * no user process associated with it (FL_NOUSER), but we won't throw the
 * entry away. Otherwise, we must on the leave of the sequencer declare another
 * member has sequencer.
 */

    register hist_p h;
    group_p g_var;
    register group_p g;
    f_size_t rs;
    errstat err;

    BEGIN_MEASURE(grp_begin_rcv);
    if ((err = grp_find(gd, &hdr->h_port, &g_var)) != STD_OK) {
	return err;
    }
    g = g_var;

    /* Sanity checks. */
    if(g->g_flags & FL_NOUSER) return(BC_NOEXIST);
    if(g->g_flags & FL_RECEIVING) { /* already receiving? */
	progerror();
	return(BC_FAIL);
    }

    if(cnt > 0 && buf == 0)  
	return(STD_ARGBAD);

    if(g->g_index != g->g_seqid) g->g_seq->m_retrial = g->g_maxretrial;

    /* Because a msg has been delivered, there could be room in the history to
     * accept new ones. Check this.
     */
    if(g->g_nexthistory > g->g_nextseqno && g->g_index == g->g_seqid && 
       !HST_SYNCHRONIZE(g))
	processreq(g);

    g->g_flags |= FL_RECEIVING;
    while(!HST_IN(g, g->g_me->m_expect)) {
	/* The following test is ok; we want to give a member a chance
	 * to receive the messages that are buffered in the history, but
	 * it should not block.
	 */
	if(g->g_flags & FL_RESET) {
	    g->g_flags &= ~FL_RECEIVING;
	    return(BC_ABORT);
	}
	assert(g->g_flags & FL_RECEIVING);
	END_MEASURE(grp_begin_rcv);
	if(await_reason((event) &g->g_receiver, 0L, "grp_receive") < 0) {
	    g->g_flags &= ~FL_RECEIVING;
	    return(BC_ABORT);
	}
	BEGIN_MEASURE(grp_end_rcv);
	assert(g->g_flags & FL_RECEIVING);
    }
    g->g_flags &= ~FL_RECEIVING;

    /* There is a msg to be delivered to the user application. */
    h = &g->g_history[HST_MOD(g, g->g_me->m_expect)]; 	/* get the msg */
    compare(h->h_size, >=, sizeof(header));
    memmove((_VOIDSTAR) hdr, (_VOIDSTAR) h->h_data, sizeof(header));
    rs = (h->h_bc.b_type == BC_JOIN || h->h_bc.b_type == BC_RESULT) ? 0 : 
	MIN(cnt, h->h_size-sizeof(header));
    memmove((_VOIDSTAR) buf, (_VOIDSTAR) (h->h_data + sizeof(header)),
								(size_t) rs);
    g->g_me->m_expect++;	/* the message is delivered */
    if(more != 0) *more = g->g_nextseqno - g->g_me->m_expect;
    if(g->g_flags & FL_DESTROY && h->h_bc.b_type == BC_LEAVE &&
       h->h_bc.b_cpu == g->g_index) {
	/* last message */
	if(g->g_me == g->g_seq) g->g_flags |= FL_NOUSER;
	else deletegroup(g);
    }
    END_MEASURE(grp_end_rcv);
    return (long) rs;
}


/* Functions for recovery from member crashes. If a member thinks that another
 * member has crashed (e.g, when grp_receive fails), it initiates the
 * recovery by calling ResetGroup. If another member is coordinator of the
 * recovery, this member becomes a cohort. If not, it starts as coordinator.
 * A cohort polls the coordinator once in while to check if it is still alive.
 * The coordinator sends once in while a message on which it requires an
 * acknowledgement. If a cohort or a coordinator crashes, the recovery starts
 * again from the beginning.
 */

static int cohort(g)
    group_p g;
{
/* A cohort waits until it hears from the coordinator what the new group will
 * be.
 */

    pkt_p msg;
    
    assert(!(g->g_flags & FL_COORDINATOR));
    g->g_coord->m_retrial = g->g_maxretrial;
    while(g->g_flags & FL_RESET) {
	g->g_resettimer = g->g_resettimeout;
	if (await_reason((event) &g->g_reset, 0L, "cohort") < 0) {
	    GDEBUG(LR, printf("cohort: await failed\n"));
	    g->g_flags &= ~(FL_VOTE | FL_COHORT);
	    return(BC_FAIL);
	}
	if(g->g_flags & FL_RESET) {
	    GDEBUG(LM, printf("cohort(%d): timeout\n", group_no(g)));
	    if(g->g_coord->m_retrial-- <= 0){
		g->g_flags &= ~(FL_VOTE | FL_COHORT);
		assert(!(g->g_flags & FL_COORDINATOR));
		return(BC_ABORT);
	    }
	    STINC(st_svote);
	    if(fillmsg(g, &msg, (header_p) 0, BC_ALIVEREQ, (g_seqcnt_t) A_PTP,
		       0L, g->g_index, (char *) 0, 0L)) {
		STINC(st_salivereq_1);
		do_flip_unicast(g->g_ifno, msg, 0, &g->g_coord->m_addr,
				g->g_me->m_ep, (f_size_t) sizeof(bchdr_t),
				g->g_ltimeout, "cohort");
	    }
	}
    }
    assert(!(g->g_flags & FL_COORDINATOR));
    g->g_coord = 0;
    g->g_flags &= ~FL_COHORT;
    if(g->g_flags & FL_RESET) return(BC_FAIL);
    else return(g->g_total);
}


static int coordinator(g, n, uhdr)
    group_p g;
    g_index_t n;
    header_p uhdr;
{
/* The coordinator protocol contains of two phases. In the first phase, it
 * invites other members to join the recovery. Every invited member responds
 * and tells the coordinator if goes along and what its highest sequence
 * number is. At the end of the first phase, the coordinator collects
 * possibly missing messages, and then informs all cohorts about the new
 * group. After it has received an ack from all cohorts, it becomes the
 * new sequencer and enters normal operation again.
 */

    f_size_t size;
    int success;
    g_index_t voter;
    bchdr_t bc_hdr;
    char *buf, *p;
    member_p m;
    
    for(m = g->g_member; m < g->g_member + g->g_maxgroup; m++) {
	if(m->m_retrial >= 0) m->m_retrial = g->g_maxretrial;
	m->m_replied = 0;
        m->m_vote = SILENT;
    }
    g->g_me->m_vote = g->g_nextseqno;
    g->g_me->m_replied = 1;
    if(!crash(g)) g->g_flags |= FL_VOTE;
    while(g->g_flags & FL_VOTE && g->g_flags & FL_COORDINATOR) {
	sendreform(g);
	if(g->g_flags & FL_VOTE && g->g_flags & FL_COORDINATOR) {
	    if (await_reason((event) &g->g_reset, 0L, "coordinator") < 0) {
	        GDEBUG(LM, printf("coordinator: await failed\n"));
		g->g_flags &= ~FL_COORDINATOR;
		return(BC_FAIL);
	    }
	}
	if(g->g_flags & FL_VOTE && g->g_flags & FL_COORDINATOR) {
	    GDEBUG(LM, printf("coordinator (vote) : timeout\n", group_no(g)));
	    for(m=g->g_member; m < g->g_member+g->g_maxgroup; m++) {
		if(!IS_MEMBER(m->m_state)) continue;
		if(!m->m_replied) m->m_retrial--;
		if(crash(g)) g->g_flags &= ~FL_VOTE;
	    }
	}
    }
    GDEBUG(LM, printf("coordinator(%d): voting is done\n", group_no(g)));
    if(g->g_flags & FL_COHORT) {
	GDEBUG(LM, printf("coordinator(%d): somebody took over\n",
			  group_no(g)));
	assert(!(g->g_flags & FL_COORDINATOR));
	return cohort(g);
    }
    /* from now on we can not use the old group address.
     */
    GDEBUG(LM, printf("coordinator(%d): %d vote(s) in favor\n", group_no(g),
		      numbervotes(g, (g_index_t *) 0))); 
    success = numbervotes(g, &voter) >= n;
    if(g->g_member[voter].m_vote != g->g_coord->m_vote) {
	success = collectmsg(g, voter, (g_seqcnt_t) g->g_member[voter].m_vote);
	g->g_coord->m_vote = g->g_nextseqno;
    }
    if(success) success = update_members(g);
    if(success) {
	assert(g->g_member[voter].m_vote == g->g_coord->m_vote);
	buildgroup(g);
	if((g->g_ep_all = flip_register(g->g_ifno, &g->g_prvaddr_all)) < 0)
		panic("result: flip_register failed");
	if((g->g_bcastep = flip_register(g->g_ifno, &nulladdr)) < 0) 
	    panic("grp_create: flip_register bcast failed");
	GRP_GETBUF(g, buf, g->g_maxctrlsize);
	if (buf == 0) {
	    /* Shouldn't happen; when resiliency is non-zero every member
	     * is supposed to have enough buffers.
	     */
	    GDEBUG(LR, printf("coordinator: no buffer\n"));
	    return(BC_FAIL);
	}
	p = grp_marshall_result(buf, buf + g->g_maxctrlsize, g, uhdr);
	if (p == NULL) {
	    GDEBUG(LR, printf("coordinator: marshalling errorr\n"));
	    return(BC_FAIL);
	}
	size = p - buf;
	bc_hdr.b_cpu = g->g_index;
	bc_hdr.b_messid = g->g_me->m_messid;
	bc_hdr.b_seqno = g->g_nextseqno;
	bc_hdr.b_type = BC_RESULT;
	/* Update minhistory; the sequencer never accepts more g_nhstfull - 1
	 * messages in its history. Update minhist to confirm to the minhist
	 * of a sequencer.
	 */
	if(g->g_nextseqno > (g->g_nhstfull -1))
	    g->g_minhistory = MAX(g->g_nextseqno - (g->g_nhstfull-1),
				  g->g_minhistory);
	compare(g->g_me->m_expect, >=, g->g_minhistory);
	(void) hst_append(g, &bc_hdr, buf, size, BC_BIGENDIAN);
    } else {
	buf = 0;
	size = 0;
    }
    if(g->g_total > 1) restartgroup(g);
    while(g->g_flags & FL_RESULT) {
	sendresult(g, success, buf, size);
	if (await_reason((event) &g->g_reset, 0L, "coordinator2") < 0) {
	    GDEBUG(LM, printf("coordinator: await failed\n"));
	    g->g_flags &= ~FL_COORDINATOR;
	    return(BC_FAIL);
	}
	if(g->g_flags & FL_RESULT) {
	    GDEBUG(LM, printf("coordinator(%d): result timeout\n", 
			      group_no(g)));
	    for(m=g->g_member; m < g->g_member+g->g_maxgroup; m++) {
		if(!IS_MEMBER(m->m_state)) continue;
		if(!m->m_replied) m->m_retrial--;
	    }
	    if(crash(g)) {
		GDEBUG(LM, printf("coord (result): failed\n"));
		g->g_flags &= ~FL_RESULT;
	    }
	}
    }
    GDEBUG(LM, printf("coordinator(%d) is finished; success = %d; n = %d\n", 
		      group_no(g), success, n));
    if(success) {
	g->g_flags &= ~FL_RESET;
	setsynctimer(g);
	setalivetimer(g);
	g->g_nextseqno++;
	g->g_me->m_messid++;
	if(g->g_flags & FL_SENDING) {
	    g->g_flags |= FL_FAILED;
	    g->g_flags &= ~FL_SENDING;
	    wakeup((event) &g->g_sender);
	}
    }
    g->g_flags &= ~FL_COORDINATOR;
    g->g_coord = 0;
    return(success ? (int) g->g_total : BC_FAIL);
}


g_indx_t grp_reset(gd, hdr, n)
    g_id_t gd;
    header_p hdr;
    g_indx_t n;
{
/* Start the recovery. If it succeeds to make a new group with at least n
 * members, it returns the actual group size. It fails when not enough
 * remainging members can be found to make a new group.
 */

    g_indx_t r;
    group_p g;
    errstat err;
    
    if ((err = grp_find(gd, &hdr->h_port, &g)) != STD_OK) {
	return err;
    }

    /* possibly recovering after a signal */
    g->g_flags &= ~FL_SIGNAL;
    if(!(g->g_flags & FL_RESET)) 	/* is rececovery needed? */
	return((n <= g->g_total) ? g->g_total : BC_ILLRESET);
    /* Already busy doing recovery? */
    if((g->g_flags & FL_COORDINATOR) || (g->g_flags & FL_COHORT))
	return(BC_ILLRESET);
    do {
	if(g->g_flags & FL_VOTE) {
	    GDEBUG(LM, printf("grp_reset(%d): %d is coordinator\n",
			      group_no(g), coord_mem_no(g)));
	    g->g_flags |= FL_COHORT;
	    assert(g->g_coord != 0);
	    r = cohort(g);
	    assert(!(g->g_flags & FL_COHORT));
	}
	else {
	    g->g_flags |= FL_COORDINATOR;
	    g->g_coord = g->g_me;
	    GDEBUG(LM, printf("grp_reset(%d): %d is coordinator\n",
			      group_no(g), coord_mem_no(g))); 
	    r = coordinator(g, (g_index_t) n, hdr);
	    assert(!(g->g_flags & FL_COORDINATOR));
	}
    } while(r == BC_ABORT);
    return r;
}
