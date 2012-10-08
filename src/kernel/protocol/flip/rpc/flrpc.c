/*	@(#)flrpc.c	1.22	96/03/07 15:29:08 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Some explanation of the ifdefs is required.
 *
 *  UNIX - defined for all versions of SunOS.  When not defined, Amoeba
 *	   is assumed to be the target.
 *  UNIX_STREAMS - defined for SunOS 5.x and probably other SYS VR4 -like
 *	   unix systems.  Sleep/wakeup cannot be used in the streams-based
 *	   version of the driver.
 */

#if RPC_PROFILE
#include <_ARGS.new.h>
#endif /* RPC_PROFILE */

#ifdef UNIX_STREAMS
#define kid_t	amoeba_kid_t
#define getblk	amoeba_getblk
#define ffs	amoeba_ffs
#define panic	printf
struct thread;
#endif /* UNIX_STREAMS */

#include "amoeba.h"
#include "stderr.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "sys/flip/flip_proto.h"    
#if RPC_PROFILE
#include <sys/profile.h>
#endif /* RPC_PROFILE */
#include "sys/flip/flrpc_port.h"
#include "flrpc_kid.h"
#include "group.h"
#include "string.h"

#include "machdep.h"
#include "global.h"
#include "exception.h"

#if defined(UNIX) && !defined(UNIX_STREAMS)
#include "ux_int.h"
#include "ux_rpc_int.h"
#endif /* UNIX */

#include "server/process/proc.h"
#define MPX	1	/* need this for a thread's signal state */
#include "kthread.h"
#include "sys/flip/group.h"

#include "byteorder.h"
#include "rpc_endian.h"
#include "am_endian.h"

#include "sys/proto.h"    
#include "sys/flip/measure.h"
#include "sys/flip/flrpc.h"
#include "module/fliprand.h"

#ifdef RPC_DEBUG
#include "../sys/routtab.h"

#if defined(UNIX) || defined(NO_OUTPUT_ON_CONSOLE)
#define console_enable(x)	/**/
#endif /* UNIX || NO_OUTPUT_ON_CONSOLE */

#ifdef PRINTF_LEVEL
/* don't print to serial output to avoid interrupt queue overflow */
#undef DPRINTF
#define DPRINTF(x, y)		\
    if ((x) <= PRINTF_LEVEL) {	\
	console_enable(0);	\
    	printf y;		\
	console_enable(1);	\
    } else                      \
        /* do nothing */;
#endif /* PRINTF_LEVEL */
#endif /* RPC_DEBUG */

#ifdef UNIX_STREAMS
#undef lobyte
#undef hibyte
#undef timeout
#undef kid_t
#undef getblk
#undef ffs
#define printf	unix_printf
#define threadp unix_threadp
#include "ux_int.h"
#include "ux_rpc_int.h"
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include "fldefs.h"
#undef printf
#undef threadp
#undef curthread

/* The UNIX_STREAMS version must NOT issue a FLIP_SYNC anywhere. */
#undef	FLIP_SYNC
#define	FLIP_SYNC 0

#endif /* UNIX_STREAMS */

#define MON_EVENT(str)	DPRINTF(0, ("%s\n", str))

#ifdef UNIX
/* Hacks to get this code running as a unix driver. */
#define	getpid			uxrpc_getpid
#define getmilli		uxrpc_getmilli
#define wakeup(v)		uxrpc_wakeup(st->st_thread, v)
#define await_reason(v, d, s)	uxrpc_await(st->st_thread, v, d)
#ifdef UNIX_STREAMS
#define progerror		printf("line %d: ", __LINE__); uxrpc_progerror
#else
#define progerror		uxrpc_progerror
#endif
#define userthread		uxrpc_userthread

#define uunmap(t, a, s) 
#define putsig			uxrpc_putsig
#define nproc			am_nproc

#ifndef UNIX_STREAMS
extern void uxrpc_buffer();
#endif /* UNIX_STREAMS */
extern void uxrpc_progerror();
extern void uxrpc_cleanup();
extern unsigned long uxrpc_getmilli();

static int pkt_keep;
extern uint16 am_nproc;
extern struct rpc_device rpc_dev[];
#endif /* UNIX */

/* This module implements the amoeba remote operations on top of flip.
 * It exports a couple of routines: 
 *	1) rpc_getreq()
 *	2) rpc_trans()
 *	3) rpc_putrep()
 *	4) grp_forward()
 *	5) rpc_cleanup()
 *	6) rpc_timeout()
 *
 * This interface is almost the same as the old transaction interface, but will
 * be changed in the future. The timeout-call will be merged with rpc_trans, and
 * a new call will be introduced: set_port_list().
 * The major difference between the old interface and this one is that 
 * request and replies may be of any size and a new primitive (grp_forward).
 *
 * The implementation guarantuees at-most-once semantics.
 * The basic protocol takes 4 messages (request, acknowledgement for request,
 * reply, acknowledgement of reply), but the implementation tries to avoid
 * sending the acknowledgement on the request. To achieve at-most-once
 * semantics all transactions from a kernel (identified by a kid) are
 * numbered in a strict-increasing order (see AMOR document).
 *
 * Because replies and requests are possibly very large, the protocol does
 * not retransmit the complete message after a timeout. It first asks the other
 * side if it really did not receive the message. If it gets an negative
 * acknowledgement back, the message is retransmitted.
 *
 * Processes are addresses by a flip address. Threads are addressed by a tuple
 * containing a flip address and an index into the state table. Thus, a port
 * plus flip-address uniquely identifies a server; the port only identifies
 * a service.
 *
 * Currently RPCs are only allowed to cross trusted networks.
 */


extern uint16 totthread;	/* for each thread there is state entry */
extern uint16 nproc;		/* for each process there is a state entry */
extern f_hopcnt_t max_hops;	/* maximum diameter of network topology */
extern uint16 rpc_pktsize;	/* size of direct data for rpc packets. */
extern uint16 rpc_npkt;		/* number of packets in the rcp pool. */


#ifndef UNIX_STREAMS
static
#endif /* UNIX_STREAMS */
       state_p statetab;	/* table with rpc state information */
static procstate_p proctab;	/* table with rpc state per process */
static adr_t mykid;		/* kernel identifier for this kernel */
static long mytsn;		/* transaction sequencer number */
static uint16 nthread_proc;	/* number of threads per process */
static int timerrunning;	/* Is there a timer running? */
static pool_t rpc_pool;		/* pool with rpc packets */
static pkt_p rpc_pkt;
static char *rpc_pktdata;
static adr_t nulladdr;
static int rpc_initialised;	/* set once the rpc layer is initialised */

#ifdef UNIX_STREAMS
static void wakeup_locate(state_p st);
static void sendrep(state_p st);
static void deliverreq(state_p st, uint8 amflags, int flflags);
void wakeup_putreq(state_p st);
void wakeup_getrep(state_p st);
#endif /* UNIX_STREAMS */
static void sendreceived _ARGS((state_p st, adr_p da, uint16 dest,
				uint16 from, uint32 tid, uint8 am_flag));
static void sendreq _ARGS(( state_p st, uint8 amflags, int flflags));

#ifdef STATISTICS
#define SIZEBINSIZE     20      /* Actually log2(MAXCNT+1)+1 */
typedef struct amstat {
    int	rpc_srequest;
    int	rpc_sreply;
    int	rpc_sack;
    int	rpc_snak;
    int	rpc_slocate;
    int	rpc_shereis;
    int	rpc_salive;
    int	rpc_senquire;
    int	rpc_sreceived;
    int rpc_sforward;
    int rpc_sfail;
    int	rpc_rrequest;
    int	rpc_rreply;
    int	rpc_rack;
    int	rpc_rnak;
    int	rpc_rlocate;
    int	rpc_rhereis;
    int	rpc_ralive;
    int	rpc_renquire;
    int	rpc_rreceived;
    int rpc_rforward;
    int rpc_rfail;
    int	rpc_timeout;
    int rpc_localreq;
    int rpc_grqbin[SIZEBINSIZE];
    int rpc_prpbin[SIZEBINSIZE];
    int rpc_prqbin[SIZEBINSIZE];
    int rpc_grpbin[SIZEBINSIZE];
} amstat_t, *am_stat_p;

static amstat_t amstat;

static
twologenplusone(n)
f_size_t n;
{
        register log;

        log = -1;
        n++;
        do {
                n >>= 1;
                log++;
        } while(n != 0);
        compare(log, >=, 0);
        if (log >= SIZEBINSIZE) {
		log = SIZEBINSIZE - 1;
	}
        return log;
}

#define STINC(type)	(amstat.type++)
#define INCSIZEBIN(x, size)     amstat.x[twologenplusone(size)]++
#else
#define STINC(type)
#define INCSIZEBIN(x, size)
#endif /* STATISTICS */

/* Put user header in pkt. */
#define sethdr(pkt, hdr)	proto_dir_append(pkt, hdr, sizeof(header))


#if defined(DEBUG)
/* Debug stuff. */

static void printtype(t)
    uint8 t;
{
    switch(t) {
    case RPC_LOCATE:
	printf("locate");
	break;
    case RPC_HEREIS:
	printf("hereis");
	break;
    case RPC_REQUEST:
	printf("request");
	break;
    case RPC_REPLY:
	printf("reply");
	break;
    case RPC_ACK:
	printf("ack");
	break;
    case RPC_NAK:
	printf("nak");
	break;
    case RPC_ENQUIRE:
	printf("enquire");
	break;
    case RPC_ALIVE:
	printf("alive");
	break;
    case RPC_RECEIVED:
	printf("received");
	break;
    case RPC_FORWARD:
	printf("forward");
	break;
    case RPC_FAIL:
	printf("fail");
	break;
    default:
	printf("unkown am type: %d\n", t);
	break;
    }
}


static void printamhdr(amp)
    amphdr_p amp;
{
    printf("to: %d from %d ", amp->ah_dest, amp->ah_from);
    printtype(amp->ah_type);
    printf(" flags %d tid %D\n", amp->ah_flags, amp->ah_tid);
}
#endif /* defined(DEBUG) */


/* Fill protocol header */
static void setamp(pkt, dst, fr, t1, f1, td, k, p)
    pkt_p pkt;
    uint16 dst, fr;
    uint8 t1, f1;
    uint32 td;
    adr_p k;
    port *p;
{
    register amphdr_p amh;
    
    PROTO_ADD_HEADER(amh, pkt, amphdr_t);
    amh->ah_dest = dst;
    amh->ah_from = fr;
    amh->ah_type = t1;
    amh->ah_flags = f1;
    amh->ah_tid = td;
    if(k != 0) amh->ah_kid = *k;
    if(p != 0) amh->ah_port = *p;
    rpc_setendian(amh);
    proto_fix_header(pkt);
    pkt->p_admin.pa_priority = (amh->ah_type == RPC_REQUEST || amh->ah_type 
				== RPC_REPLY) ? 0 : 1;
}


/* This routine sets the timers. It is called when a packet is returned to the
 * packet pool (e.g, after the packet is sent on the network.)
 */
static void rpc_settimer(arg)
    long arg;
{
    state_p st = (state_p) arg;

    assert(st >= statetab && st < statetab + totthread);

    switch(st->st_state) {
    case IDLE:
	return;
    case LOCATE:
	assert(st->st_deltatime > 0);
	st->st_timeout = st->st_deltatime;
	timerrunning = 1;
	break;
    case PUTREQ:
	assert(st->st_srtime > 0);
	st->st_timeout = st->st_srtime + SERVETIMEOUT;
	timerrunning = 1;
	break;
    case PUTREP:
	assert(st->st_crtime > 0);
	{
	    int t = REPLYTIMEOUT(st->st_crtime);

	    if (t <= SWEEPINTERVAL) {
		t = SWEEPINTERVAL + 1;
	    } else {
		if (t > MAXSERVERREPLY) {
		    t = MAXSERVERREPLY;
		}
	    }
	    st->st_timeout = t;
	}
	timerrunning = 1;
	break;
    case GETREP:
	st->st_timeout = CRASHTIME;
	timerrunning = 1;
	break;
    }

    /*
     * Make sure the timeout is at least one sweep interval!
     * Otherwise the time can be much too short
     */
    
    if(st->st_flag & F_REASSEM) {
	assert(st->st_state == GETREQ || st->st_state == PUTREQ || st->st_state == GETREP);
	st->st_reassemble = MAXREASSEMBLE;
	timerrunning = 1;
    }
}


/* Is there a busy server servicing a request from client <sa, node> with 
 * transaction id tid?
 */
static state_p find(sa, node, tid)
    adr_p sa;
    uint16 node;
    uint32 tid;
{
    register state_p st;
    
    for(st = statetab; st < statetab + totthread; st++) {
	if(SERVING(st) && ADR_EQUAL(&st->st_caddr, sa) && st->st_ctid
	   == tid && st->st_cident == node) {
	    return(st);
	}
    }
    return(0);
}


/*
 * Find server that listens to port p. At the same time, we determine very
 * roughly how long it takes to get to the server. When the locate goes out,
 * a timestamp is put on the locate, the hereis from the server contains
 * the same timestamp. The difference between the current time and the timestamp
 * is stored in the portcache and is used to set timer values.
 * Multiple servers may answer the locate, we store each (port,flip) tuple
 * in the port cache, but we take the first one that responds.
 */

#ifdef UNIX_STREAMS

static int findserver(st, p, lochcnt)
    state_p st;
    port *p;
    f_hopcnt_t lochcnt;
{
    register pkt_p msg;
    long *protocol;
    int r;
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;

    st->st_deltatime = LOCTIMEOUT;

    if (!(st->st_cache = port_lookup(p))) {
	if(st->st_flag & F_SIGNAL) {
	    MON_EVENT("findserver: locate terminated due to signal");
	    assert(st->st_state == IDLE);
	    assert(st->st_timeout < 0);
	    st->st_flag &= ~(F_SIGNAL | F_RETRANS);
	    rpcfd = &rpc_dev[st - statetab];
	    mp = rpcfd->mp;
	    freemsg(rpcfd->mp_data);
	    rpcfd->mp_data = 0;
	    rpc_args = (struct rpc_args *) DB_BASE(mp);
	    rpc_args->rpc_status = RPC_ABORTED;
	    mp->b_rptr = DB_BASE(mp);
	    mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	    qreply(rpcfd->queue, mp);
	    return(0);
	}
	st->st_state = LOCATE;
	PKT_GET(msg, &rpc_pool);
	if(msg == 0) {
	    DPRINTF(0, ("WARNING: findserver: out of packets"));
	    rpc_settimer((long) st);	/* wait some time and try again. */
	} else {
	    proto_init(msg);
	    pkt_set_release(msg, rpc_settimer, (long) st);
	    setamp(msg, 0, st->st_index, RPC_LOCATE, 0, (uint32) getmilli(),
								(adr_p) 0, p);
	    STINC(rpc_slocate);
	    PROTO_ADD_HEADER(protocol, msg, long);
	    *protocol = FLIP_AMRPC;
	    enc_l_be(protocol);	/* send protocol field in network order. */
	    proto_fix_header(msg);
	    r = flip_broadcast(st->st_proc->ps_ifno, msg, st->st_proc->ps_ep,
			   (f_size_t) sizeof(amphdr_t) + sizeof(long), lochcnt);
	    if(r < 0) {
		DPRINTF(0, ("find_server: flip_broadcast failed %d\n", r));
		pkt_discard(msg);
	    }
	    
	}
	rpcfd = &rpc_dev[st - statetab];
	if(st->st_state == LOCATE && rpcfd->loctime >= st->st_ltime) {
	    DPRINTF(0, ("findserver: %d(%d) locate failed\n", 
			THREADSLOT(st->st_thread), st->st_index));
	    MAKE_IDLE(st);
	    st->st_flag &= ~(F_SIGNAL | F_RETRANS);
	    st->st_rhdr = 0;
	    rpcfd = &rpc_dev[st - statetab];
	    mp = rpcfd->mp;
	    freemsg(rpcfd->mp_data);
	    rpcfd->mp_data = 0;
	    rpc_args = (struct rpc_args *) DB_BASE(mp);
	    rpc_args->rpc_status = RPC_NOTFOUND;
	    mp->b_rptr = DB_BASE(mp);
	    mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	    qreply(rpcfd->queue, mp);
	    return(0);
	}
    } else {
	st->st_retrial = MAX_RPC_RETRIAL;
	rpcfd = &rpc_dev[st - statetab];
	rpcfd->lochcnt = 1;
	rpcfd->loctime = 0;
	rpcfd->maxretrial = LOCRETRIAL;
	MAKE_IDLE(st);
	return(1);
    }
    return(0);
}

#else /* UNIX_STREAMS */

static int findserver(st, p)
    state_p st;
    port *p;
{
    register pkt_p msg;
    f_hopcnt_t lochcnt = 1;
    int loctime = 0;
    int maxretrial = LOCRETRIAL;
    long *protocol;
    int r;

    st->st_deltatime = LOCTIMEOUT;

    while(!(st->st_cache = port_lookup(p))) {
	st->st_state = LOCATE;
	PKT_GET(msg, &rpc_pool);
	if(msg == 0) {
	    DPRINTF(0, ("WARNING: findserver: out of packets"));
	    rpc_settimer((long) st);	/* wait some time and try again. */
	} else {
	    proto_init(msg);
	    pkt_set_release(msg, rpc_settimer, (long) st);
	    setamp(msg, 0, st->st_index, RPC_LOCATE, 0, (uint32) getmilli(),
								(adr_p) 0, p);
	    STINC(rpc_slocate);
	    PROTO_ADD_HEADER(protocol, msg, long);
	    *protocol = FLIP_AMRPC;
	    enc_l_be(protocol);	/* send protocol field in network order. */
	    proto_fix_header(msg);
	    DPRINTF(1, ("Send away LOCATE\n"));
	    r = flip_broadcast(st->st_proc->ps_ifno, msg, st->st_proc->ps_ep,
			   (f_size_t) sizeof(amphdr_t) + sizeof(long), lochcnt);
	    if(r < 0) {
		DPRINTF(0, ("find_server: flip_broadcast failed %d\n", r));
		pkt_discard(msg);
	    }
	    
	}
	if(st->st_state == LOCATE) {
	    (void) await_reason((event) &st->st_state, 0L, "findserver");
	    loctime += st->st_deltatime;
	}
	if(st->st_flag & F_SIGNAL) {
	    MON_EVENT("findserver: locate terminated due to signal");
	    assert(st->st_state == IDLE);
	    assert(st->st_timeout < 0);
	    return(0);
	}
	if(--maxretrial == 0) {
	    if(lochcnt < max_hops)
		lochcnt = MIN((uint16) (2 * lochcnt + 1), max_hops);
	    else st->st_deltatime *= 2;
	    maxretrial = LOCRETRIAL;
	}
	if(st->st_state == LOCATE && loctime >= st->st_ltime) {
	    DPRINTF(0, ("findserver: %d(%d) locate failed\n", 
			THREADSLOT(st->st_thread), st->st_index));
	    MAKE_IDLE(st);
	    return(0);
	}
    }
    MAKE_IDLE(st);
    return(1);
}

#endif /* UNIX_STREAMS */


/* Send protocol message. The timeout value is set 0, because these
 * message are sent as a reply on incomming message.
 * Also, these message do not have to travel over only trusted networks.
 */
static void sendamh(p, da, to, from, type, tid, kid)
    procstate_p p;
    adr_p da;
    uint16 to, from;
    uint8 type;
    uint32 tid;
    adr_p kid;
{
    pkt_p msg;
    int r;

    PKT_GET(msg, &rpc_pool);
    if(msg == 0) {
	DPRINTF(0, ("WARNING: send protocol : out of packets"));
    } else {
	proto_init(msg);
	setamp(msg, to, from, type, 0, tid, kid, (port *) 0); 
	r = flip_unicast(p->ps_ifno, msg, 0, da, p->ps_ep,
				(f_size_t) sizeof(amphdr_t), (interval) 0);
	if(r < 0) {
	    DPRINTF(0, ("sendamh: flip_unicast -> %d\n", r));
	    pkt_discard(msg);
	}
    }
}


static void sendenquire(st, flip_flag)
    state_p st;
    int flip_flag;
{
    pkt_p msg;
    uint8 f;
    int r;

    f = (st->st_flag & F_SIGNAL) ? FL_SIGTRANS : 0;
    flip_flag |= st->st_security;
    PKT_GET(msg, &rpc_pool);
    if(msg == 0) {
	DPRINTF(0, ("WARNING: sendenquire: out of packets"));
    } else {
	proto_init(msg);
	pkt_set_release(msg, rpc_settimer, (long) st);
	setamp(msg, st->st_sident, st->st_index, RPC_ENQUIRE, f, 
	       st->st_mytid, (adr_p) 0, (port *) 0);
	STINC(rpc_senquire);
	r = flip_unicast(st->st_proc->ps_ifno, msg, flip_flag, &st->st_saddr,
		 st->st_proc->ps_ep, (f_size_t) sizeof(amphdr_t), st->st_ltime);
	if(r < 0) {
	    DPRINTF(0, ("sendenquire: flip_unicast -> %d\n", r));
	    pkt_discard(msg);
	}
    }
}


/* A locate for a port arrived. */
static void gotlocate(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    register amphdr_p amp;
    state_p st, *t;
    pkt_p msg;
    int r;
    
    proto_cur_header(amp, pkt, amphdr_t);
    for(t = proc->ps_thread; t < proc->ps_thread + nthread_proc; t++) {
	st = *t;
	if(st == 0) continue;
	if(st->st_state == GETREQ && PORTCMP(&amp->ah_port, &st->st_pubport)) {
	    /* the port is here */
	    PKT_GET(msg, &rpc_pool);
	    if(msg == 0) {
		DPRINTF(0, ("WARNING: gotlocate: out of packets"));
	    } else {
		proto_init(msg);
		setamp(msg, amp->ah_from, st->st_index, RPC_HEREIS, 0,
		       amp->ah_tid, (adr_p) 0, &amp->ah_port); 
		STINC(rpc_shereis);
		/* Send hereis without security back. */
		r = flip_unicast(st->st_proc->ps_ifno, msg, 0, sa,
			 st->st_proc->ps_ep, (f_size_t) sizeof(amphdr_t),
			 (interval) 0); 
		if(r < 0) {
		    DPRINTF(0, ("gotlocate: flip_unicast -> %d\n", r));
		    pkt_discard(msg);
		}
	    }
	    return;
	}
    }
}


/* A hereis for a port arrived */
static void gothereis(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    amphdr_p amp;
    state_p st;
    unsigned long time;
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
#endif /* UNIX_STREAMS */
    
    proto_cur_header(amp, pkt, amphdr_t);
    assert(amp);
    st = RPC_STATE(proc, amp->ah_dest);
    assert(st == 0 || (st >= statetab && st < statetab + totthread));
    time = getmilli() - amp->ah_tid;

    if(time <= 0) time = (unsigned long) SWEEPINTERVAL;
    if(time > 1000) time = 1000; /* tmp HACK */

    port_install(amp->ah_from, sa, &amp->ah_port, P_SOMEWHERE, (interval) time,
								(char *) 0);
    if(st && st->st_state == LOCATE && PORTCMP(&st->st_sport, &amp->ah_port)) {
	/* we are looking for the port */
	st->st_retrial = MAX_RPC_RETRIAL;
#ifdef UNIX_STREAMS
	rpcfd = &rpc_dev[st - statetab];
	rpcfd->lochcnt = 1;
	rpcfd->loctime = 0;
	rpcfd->maxretrial = LOCRETRIAL;
	MAKE_IDLE(st);

	if (st->st_cache = port_lookup(&st->st_sport)) {
	    deliverreq(st, 0, 0);
	}
#else /* UNIX_STREAMS */
	MAKE_IDLE(st);
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
    }
}


#ifndef SECURITY
/*ARGSUSED*/
#endif
static void acceptreq(st, pkt, kid, sa, length, complete, messid, bigendian,
		      flag)
    register state_p st;
    pkt_p pkt;
    kid_p kid;
    adr_p sa;
    int complete;
    f_size_t length, messid;
    int bigendian;
    int flag; /* only used if SECURITY is defined */
{
    register amphdr_p amp;
    header *hdr;
    f_size_t l;
#if !defined(UNIX) || defined(UNIX_STREAMS)
    f_size_t virsize, dirsize;
#endif /* !UNIX || UNIX_STREAMS */
#ifdef UNIX_STREAMS
    mblk_t *mp;
    struct rpc_info_t *rpc_info;
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
#endif /* UNIX_STREAMS */
    
    assert(st >= statetab && st < statetab + totthread);
    assert(st->st_state == GETREQ);
    proto_cur_header(amp, pkt, amphdr_t);
    st->st_caddr = *sa;
    st->st_ctid = amp->ah_tid;
    st->st_cident = amp->ah_from;
    st->st_ckid = amp->ah_kid;
    st->st_crtime = amp->ah_dest;

    if(st->st_crtime <= 0 || st->st_crtime > MAXSERVERREPLY)
	st->st_crtime = MAXSERVERREPLY;
    proto_remove_header(pkt);
    PROTO_LOOK_HEADER(hdr, pkt, header);
    assert(hdr);
#ifdef SECURITY
    hdr->h_signature._portbytes[0] = flag & FLIP_UNTRUSTED ? 0 : 1;
#endif
    *st->st_rhdr = *hdr;
    am_orderhdr(st->st_rhdr, bigendian);
    proto_remove_header(pkt);
    length -= sizeof(header) + sizeof(amphdr_t);
    l = MIN(st->st_rcnt, length);
    assert(l <= pkt->p_contents.pc_totsize);
    if(l > 0) {
#if !defined(UNIX) || defined(UNIX_STREAMS)
	dirsize = MIN(l, pkt->p_contents.pc_dirsize);
	virsize = l - dirsize;
	assert(st->st_rdata);
	if(dirsize > 0) {
	    (void) memmove((_VOIDSTAR) st->st_rdata,
				(_VOIDSTAR) pkt_offset(pkt), (size_t) dirsize);
	}
	if(virsize > 0) {
	    (void) memmove((_VOIDSTAR)(st->st_rdata + dirsize),
		    (_VOIDSTAR) pkt->p_contents.pc_virtual, (size_t) virsize); 
	}
#else /* !UNIX || UNIX_STREAMS */
	uxrpc_buffer(st->st_thread, pkt, st->st_rdata, l);
	assert(st->st_state == GETREQ);
	pkt_keep = 1;
#endif /* !UNIX || UNIX_STREAMS */
    }
    if(complete) {
	/* the request is complete */
	if (kid == 0) {
	    kid = kid_install(&st->st_ckid, st->st_ctid);
	} else {
	    kid_update(kid, st->st_ctid);
	}
	st->st_flag |= F_DOOP;
	assert(st->st_timeout < 0);
	assert(!(st->st_flag & F_REASSEM));
	assert(st->st_reassemble < 0);
	MAKE_IDLE(st);
	st->st_rcnt = MIN(st->st_rcnt, length);
#ifdef UNIX_STREAMS
	assert(DOOPERATION(st) || !RPC_ACTIVE(st));
	/*
	if (cnt > 0) {
	    uunmap(t, (vir_bytes) buf, (vir_bytes) cnt);
	}
	*/
	/*
	assert(st->st_rhdr == hdr);
	*/
	st->st_rhdr = 0;
#ifdef STATISTICS
	if (st->st_rcnt < 64*1024)
	    INCSIZEBIN(rpc_grqbin, st->st_rcnt);
#endif
	END_MEASURE(rpc_end_getreq);
	BEGIN_MEASURE(rpc_doop);
	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_hdr1 = rpcfd->rpc_rhdr;
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
    }
    else {
	/* reassemble the request */
	BEGIN_MEASURE(rpc_getreq_reassem);
	st->st_roffset = length;
	st->st_flag |= F_REASSEM;
	st->st_rmessid = messid;
	rpc_settimer((long) st);
	timerrunning = 1;
    }
}

#ifdef RPC_DEBUG

static void
print_adr_loc(sa)
adr_p sa;
{
    int netw;
    location loc;
    f_hopcnt_t hops;

    badr_print((char *) 0, (char *) 0, sa);
    /* the location is even more useful: */
    if (adr_route(sa, 10 /* max hops */, &netw,
		  &loc, &hops, NONTW, 0 /* safe */) == ADR_OK)
    {
	printf(" at ");
	badr_print((char *) 0, (char *) 0, &loc);
    }
    printf("\n");
}

#endif

/* A request arrived. Lookup the last transaction that we saw from this kernel.
 * If it is a retransmission, a couple of checks have to be made to ensure at-
 * most-once semantics. It is not a retransmission and the server is here, 
 * wake the server up. If the server is not here, send a nothere back.
 * If it is a retransmission; If some server is listening to the port and the
 * request has never been executed, then accept the request.  If the server
 * is busy working on the request, send back an acknowledgement.
 */
static void gotrequest(proc, pkt, sa, length, complete, messid, bigendian, flag)
    procstate_p proc;
    register pkt_p pkt;
    adr_p sa;
    f_size_t length;
    int complete;
    f_msgcnt_t messid;
    int bigendian;
    int flag;
{
    register state_p st;
    register amphdr_p amp;
    kid_p kid;
    uint16 node;

    proto_cur_header(amp, pkt, amphdr_t);
    kid = kid_lookup(&amp->ah_kid);
    if(amp->ah_flags & FL_RETRANS) {  /* a retransmission ? */
#ifdef RPC_DEBUG
	console_enable(0);
	printf("retrans from ");
	print_adr_loc(sa);
	console_enable(1);
#else
	MON_EVENT("gotrequest: retransmission");
#endif
	if((st = find(sa, amp->ah_from, amp->ah_tid)) != 0) {
	    /* server is working on it: send the ack for the request */
	    MON_EVENT("gotrequest: ack got lost");
	    assert(st >= statetab && st < statetab + totthread);
	    STINC(rpc_sack);
	    sendamh(proc, sa, amp->ah_from, st->st_index, RPC_ACK, st->st_ctid,
								    (adr_p) 0); 
	} else if(kid == 0 || kid_execute(kid, amp->ah_tid)) {
	    /* the request is new, it can be executed */
	    if(!port_match(&amp->ah_port, &proc->ps_myaddr, &node)) {
		MON_EVENT("gotrequest: the server is not here anymore");
		STINC(rpc_snak);
		sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_NAK,
						    amp->ah_tid, (adr_p) 0);  
	    } else {		/* a really new request for a known server */
		acceptreq(RPC_STATE(proc, node), pkt, kid, sa, length, complete,
			  messid, bigendian, flag);
	    }
	} else {  
	    /* no idea what happened with this request; abort rpc */
	    MON_EVENT("gotrequest: don't know what to do with the request");
	    STINC(rpc_sfail);
	    sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_FAIL,
		    amp->ah_tid, (adr_p) 0);  
	}
    } else {			/* the first time that the request arrives */
#define FIFO_NET_CHECK
#ifdef  FIFO_NET_CHECK
	/* Performs a sanity check to protect against two cases:
	 * - a retransmission arriving before the original request
	 * - a request being duplicated by a network switch (or interface).
	 *
	 * The RPC code used to depend on the network behaving as a FIFO,
	 * but the following code fixes this by checking the transaction id
	 * of original requests as well (albeit with some performance loss).
	 */
	if (kid != 0 && !kid_execute(kid, amp->ah_tid)) {
#ifndef SMALL_KERNEL
	    printf("already executed RPC %d (mess %ld) from ",
		   amp->ah_tid, messid);
#ifdef RPC_DEBUG
	    print_adr_loc(sa);
#else
	    badr_print((char *) 0, (char *) 0, &kid->k_kid);
	    printf("?\n");
#endif /* RPC_DEBUG */
#endif /* SMALL_KERNEL */
	    return;
	}
#endif /* FIFO_NET_CHECK */
	if(!port_match(&amp->ah_port, &proc->ps_myaddr, &node)) {
	    MON_EVENT("gotrequest: the server is not here anymore");
	    STINC(rpc_snak);
	    sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_NAK,
			amp->ah_tid, (adr_p) 0);
	} else {		/* a really new request for a known server */
	    acceptreq(RPC_STATE(proc, node), pkt, kid, sa, length, complete, 
		      messid, bigendian, flag);
	}
    }
}


/* A reply arrives. If destination entry is waiting for a reply, then collect
 * the reply. If the destination entry is not waiting for a reply, it is
 * apparently an old reply. Just send back an acknowledgement to the server.
*/
#ifndef SECURITY
/*ARGSUSED*/
#endif
static void gotreply(proc, pkt, sa, length, complete, messid, bigendian, flag)
    procstate_p proc;
    register pkt_p pkt;
    adr_p sa;
    f_size_t length;
    int complete;
    f_msgcnt_t messid;
    int bigendian;
    int flag; /* only used if SECURITY is defined */
{
    register state_p st;
    register amphdr_p amp;
    amphdr_p ramp;
    header *hdr;
    f_size_t l;
#if !defined(UNIX) || defined(UNIX_STREAMS)
    f_size_t dirsize, virsize;
#else
    int oldkeep;
#endif /* !UNIX || UNIX_STREAMS */
    int r;
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
#endif /* UNIX_STREAMS */

    proto_cur_header(amp, pkt, amphdr_t);
    st = RPC_STATE(proc, amp->ah_dest);
    assert(st == 0 || (st >= statetab && st < statetab + totthread));
    if(st && WAITREPLY(st) && amp->ah_tid == st->st_mytid &&
       ADR_EQUAL(&st->st_saddr, sa)) {
	proto_remove_header(pkt);
	PROTO_LOOK_HEADER(hdr, pkt, header);
	assert(hdr);
	*st->st_rhdr = *hdr;
#ifdef SECURITY
	st->st_rhdr->h_signature._portbytes[0] = flag & FLIP_UNTRUSTED ? 0 : 1;
#endif
	am_orderhdr(st->st_rhdr, bigendian);
	proto_remove_header(pkt);
	length -= sizeof(header) + sizeof(amphdr_t);
	l = MIN(st->st_rcnt, length);
	assert(l <= pkt->p_contents.pc_totsize);
	if(l > 0) {
#if !defined(UNIX) || defined(UNIX_STREAMS)
	    dirsize = MIN(l, pkt->p_contents.pc_dirsize);
	    virsize = l - dirsize;
	    assert(st->st_rdata);
	    if(dirsize > 0) {
		(void) memmove((_VOIDSTAR) st->st_rdata,
				(_VOIDSTAR) pkt_offset(pkt), (size_t) dirsize);
	    }
	    if(virsize > 0) {
		(void) memmove((_VOIDSTAR)(st->st_rdata + dirsize),
		    (_VOIDSTAR) pkt->p_contents.pc_virtual, (size_t) virsize);
	    }
#else /* !UNIX || UNIX_STREAMS */
	    uxrpc_buffer(st->st_thread, pkt, st->st_rdata, l);
	    assert(WAITREPLY(st));
	    pkt_keep = 1;
#endif /* !UNIX || UNIX_STREAMS */
	}
	if(complete) {
	    STINC(rpc_sack);
	    assert(st->st_ackpkt);
	    proto_cur_header(ramp, st->st_ackpkt, amphdr_t);
	    ramp->ah_dest = amp->ah_from;
	    rpc_setendian(ramp);
	    proto_fix_header(st->st_ackpkt);
	    END_MEASURE(rpc_ack);
#if defined(UNIX) && !defined(UNIX_STREAMS)
	    oldkeep = pkt_keep;
#endif /* UNIX || !UNIX_STREAMS */
	    r = flip_unicast(st->st_proc->ps_ifno, st->st_ackpkt,
			     st->st_security, sa, st->st_proc->ps_ep, (f_size_t)
			     sizeof(amphdr_t), st->st_ltime);
	    if(r < 0) {
		DPRINTF(0, ("gotreply: flip_unicast -> %d\n", r));
	    }
#if defined(UNIX) && !defined(UNIX_STREAMS)
	    pkt_keep = oldkeep;
#endif /* UNIX || !UNIX_STREAMS */
	    BEGIN_MEASURE(rpc_rcv_trans);
	    st->st_ackpkt = 0;
	    MAKE_IDLE(st);
	    st->st_flag &= ~(F_SIGNAL | F_REASSEM | F_RETRANS);
	    st->st_reassemble = -1;
	    st->st_rcnt = MIN(st->st_rcnt, length);
#ifdef UNIX_STREAMS
	    assert(NOCLIENT(st));
	    if (!NOCLIENT(st)) {
		printf("st->st_state = %x, st->st_flag = %x\n",
		    st->st_state, st->st_flag);
	    }
	    st->st_rhdr = 0;
#ifdef STATISTICS
	    if (st->st_rcnt < 64*1024)
		INCSIZEBIN(rpc_grpbin, st->st_rcnt);
#endif
#if RPC_PROFILE
	    repltime= getmilli();
	    replsiz= st->st_rcnt;
	    profile_rpc(getpid(t), THREADSLOT(t), &reqport, reqobj,
		    reqcommand, reqsiz, replsiz, reqtime, repltime);
#endif /* RPC_PROFILE */
	    END_MEASURE(rpc_rcv_trans);
	    END_MEASURE(rpc_trans);
	    BEGIN_MEASURE(rpc_client);

	    rpcfd = &rpc_dev[st - statetab];
	    mp = rpcfd->mp;
	    freemsg(rpcfd->mp_data);
	    rpcfd->mp_data = 0;
	    rpc_args = (struct rpc_args *) DB_BASE(mp);
	    rpc_args->rpc_hdr2 = rpcfd->rpc_rhdr;
	    rpc_args->rpc_status = st->st_rcnt;
	    mp->b_rptr = DB_BASE(mp);
	    mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	    mp->b_cont->b_rptr = DB_BASE(mp->b_cont);
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + st->st_rcnt;
	    qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	    wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
	} else {
	    BEGIN_MEASURE(rpc_trans_reassem);
	    st->st_sident = amp->ah_from;
	    st->st_roffset = length;
	    st->st_rmessid = messid;
	    st->st_flag |= F_REASSEM;
	    rpc_settimer((long) st);
	    if(st->st_state == PUTREQ) {	
		/* the request has arrived */
		st->st_timeout = -1;
		st->st_state = GETREP;
#ifndef UNIX_STREAMS
		wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
	    }
	}
    } else {
	MON_EVENT("gotreply: old reply");
	STINC(rpc_sack);
	sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_ACK, amp->ah_tid,
								    (adr_p) 0);
    }
}


/* Got acknowledgement. The acknowledgement is either for a request or a 
 * reply.
 */
static void gotack(proc, pkt, sa)
    procstate_p proc;
    register pkt_p pkt;
    adr_p sa;
{
    register amphdr_p amp;
    register state_p st;
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
#endif /* UNIX_STREAMS */
    
    proto_cur_header(amp, pkt, amphdr_t);
    assert(amp);
    st = RPC_STATE(proc, amp->ah_dest);
    if(st == 0) return;
    assert(st == 0 || (st >= statetab && st < statetab + totthread));

    if(st->st_state == PUTREQ && amp->ah_tid == st->st_mytid &&
       ADR_EQUAL(&st->st_saddr, sa)) {
	st->st_state = GETREP;
	st->st_sident = amp->ah_from;
	st->st_timeout = -1;
#ifndef UNIX_STREAMS
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
    } else if(st->st_state == PUTREP && amp->ah_tid == st->st_ctid && 
	    amp->ah_from == st->st_cident && ADR_EQUAL(&st->st_caddr,sa)) {
	MAKE_IDLE(st);
	st->st_flag &= ~F_NACKED;
#ifdef UNIX_STREAMS
	END_MEASURE(rpc_rcv_putrep);
	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = STD_OK;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
    } else if ((st->st_state == GETREQ || st->st_state == IDLE) &&
	       amp->ah_tid == st->st_ctid) {
	/* This is probably an ack on a sendreceived(), which we sent because
	 * we had to wait too long for the ack on our putreply().
	 * The latter ack did arrive after all (which caused the state change
	 * from PUTREP), so we can silently ignore this new ack.
	 */
    } else {
#ifdef RPC_DEBUG
	printf("gotack: strange ack %d (ctid %d, mytid %d, st %d, dst=%d) from ",
		amp->ah_tid, st->st_ctid, st->st_mytid, st->st_state, amp->ah_dest);
	print_adr_loc(sa);
#else /* RPC_DEBUG */
	MON_EVENT("gotack: strange ack");
#endif /* RPC_DEBUG */
    }
}


/* Got a negative acknowledgement after enquiring the destination if has
 * received the reply or request. Wake the client or server up, so that is
 * can resend the message.
*/
static void gotnack(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    amphdr_p amp;
    state_p st;
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
    int timed_out;
    uint8 amflags = 0;	/* amoeba header flag */
    int flflags = 0;	/* flip header flag */
#endif /* UNIX_STREAMS */
    
    proto_cur_header(amp, pkt, amphdr_t);
    assert(amp);
    st = RPC_STATE(proc, amp->ah_dest);
    assert(st == 0 || (st >= statetab && st < statetab + totthread));
    if(st == 0) return;

    if(st->st_state == PUTREQ && amp->ah_tid == st->st_mytid &&
       ADR_EQUAL(&st->st_saddr, sa)) {
	MON_EVENT("gotnack: request is lost");
	st->st_flag |= F_NACKED;
	st->st_timeout = -1;
#ifdef UNIX_STREAMS
	/* The server has sent a NACK indicating that it did not
	 * receive the request or is not listening to the port.
	 */
	if(st->st_retrial-- <= 0)  {  /* give up? */
	    MON_EVENT("deliverreq: server crashed");
	    st->st_rcnt = (st->st_flag & F_NACKED || st->st_flag &
			   F_UNREACHABLE) ? RPC_NOTFOUND : RPC_FAILURE;
	    MAKE_IDLE(st);
	    st->st_flag &= ~(F_SIGNAL | F_RETRANS | F_UNREACHABLE | 
			     F_NACKED | F_UNTRUSTED);
	    port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
	    rpcfd = &rpc_dev[st - statetab];
	    st->st_rhdr = 0;
	    mp = rpcfd->mp;
	    freemsg(rpcfd->mp_data);
	    rpcfd->mp_data = 0;
	    rpc_args = (struct rpc_args *) DB_BASE(mp);
	    rpc_args->rpc_status = st->st_rcnt;
	    mp->b_rptr = DB_BASE(mp);
	    mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	    qreply(rpcfd->queue, mp);
	    return;
	}

	/* If the packet is returned by a FLIP box and we are sure we have
	 * not reached the server yet, either F_UNREACHABLE is set or
	 * FLIP_INVAL is set in flflags.  If the server sends a NACK back,
	 * F_NACKED is set.  If none of these flags is set, a timer has
	 * run out.
	 */
	timed_out = (!(st->st_flag & (F_NACKED | F_UNREACHABLE)) &&
		     !(flflags & FLIP_INVAL));
	if (timed_out) {
#ifdef RPC_DEBUG
	    DPRINTF(0, ("deliverreq: send received %d\n", st->st_mytid));
#else
	    MON_EVENT("deliverreq: send received");
#endif
	    /* Check if the server has received the request. Furthermore,
	     * remember that we have sent a request that possibly could
	     * have reached the server (F_RETRANS is set).
	     */
	    st->st_flag |= F_RETRANS;
	    sendreceived(st, &st->st_saddr, st->st_sident, st->st_index,
			 st->st_mytid, FL_REQUEST);
	} else {
	    wakeup_putreq(st);
	}
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
    } else if(st->st_state == PUTREP && ADR_EQUAL(&st->st_caddr, sa)) {
	MON_EVENT("gotnack: reply got lost");
#ifndef UNIX_STREAMS
	st->st_flag |= F_NACKED;
#endif /* UNIX_STREAMS */
	st->st_timeout = -1;
#ifdef UNIX_STREAMS
	sendrep(st);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
    } else MON_EVENT("gotnack: weird nak");
}


#ifdef UNIX
#define thread_dying(t)	0
#else
#define thread_dying(t) (((t)->mx_flags & (TDS_STOP|TDS_HSIG)) != 0)
#endif

/* Are you alive? */
static void gotenquire(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    state_p st;
    amphdr_p amp;
    
    proto_cur_header(amp, pkt, amphdr_t);
    assert(amp);
    st = RPC_STATE(proc, amp->ah_dest);
    assert(st == 0 || (st >= statetab && st < statetab + totthread));
    if (st && SERVING(st) && st->st_cident == amp->ah_from &&
	ADR_EQUAL(&st->st_caddr, sa) && st->st_ctid == amp->ah_tid)
    {
	STINC(rpc_salive);
	/* It may happen that a process is stunned while it is performing
	 * an RPC with itself, so we must be careful not to send an alive
	 * message if the server thread is (presumably) dying.  Otherwise
	 * we would keep the client thread within the dying process waiting
	 * for a reply that will never come.
	 */
	if (!thread_dying(st->st_thread)) {
	    sendamh(proc, sa, st->st_cident, st->st_index, RPC_ALIVE,
		    st->st_ctid, (adr_p) 0);
	}
	if (DOOPERATION(st) && (amp->ah_flags & FL_SIGTRANS) && 
	    !(st->st_flag & F_SIGTRANS)) {   
	    /* send signal to server */
	    putsig(st->st_thread, (signum) SIG_TRANS);
	    st->st_flag |= F_SIGTRANS;
	}
    }
    /* It may happen that the client is still waiting for a reply and that the
     * server has given up sending the reply (the server tried a number of times
     * without getting an acknowledgement). The client will poll the server
     * a number of times and then give up, because it will receive no reply
     * on the enquire. To speed this process up, the server sends a fail if
     * it is not working on the request for which the client sends an enquire.
     * There are 3 cases in which this fail message has to be sent:
     * 	1) the server is not serving;
     *  2) the server is serving another client process;
     *  3) the server is serving another thread from the same client process.
     * We might also do this in case the server thread is dying, but that
     * would interfere with process migration.
     */
    else if (st && (!SERVING(st) ||
		    !ADR_EQUAL(&st->st_caddr, sa) ||
		    st->st_ctid != amp->ah_tid))
    {
        STINC(rpc_sfail);
        sendamh(proc, sa, st->st_cident, st->st_index, RPC_FAIL, amp->ah_tid,
		(adr_p) 0);
    }
}


/* The other side is alive. */
static void gotalive(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    amphdr_p amp;
    state_p st;
    
    proto_cur_header(amp, pkt, amphdr_t);
    assert(amp);
    st = RPC_STATE(proc, amp->ah_dest);
    assert(st == 0 || (st >= statetab && st < statetab + totthread));
    if(st && st->st_state == GETREP && amp->ah_from == st->st_sident && 
       ADR_EQUAL(sa, &st->st_saddr)) {
	DPRINTF(1, ("gotalive: got alive message\n"));
	st->st_retrial = MAX_RPC_RETRIAL;
    }
}


/* The other side has sent this side a message, but wonders if this side has
 * received the message. If this side did not receive the message, send
 * back a negative acknowledgement. Otherwise, a positive one.
 * The flag-field in the header tells if the message was a reply or a request.
 */
static void gotreceived(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    amphdr_p amp;
    state_p st;
    kid_p kid;
    
    proto_cur_header(amp, pkt, amphdr_t);
    assert(amp);
#ifdef RPC_DEBUG
    console_enable(0);
    printf("gotreceived (tid %d) from ", amp->ah_tid);
    print_adr_loc(sa);
    console_enable(1);
#endif
    if(amp->ah_flags & FL_REPLY) {
	st = RPC_STATE(proc, amp->ah_dest);
	assert(st == 0 || (st >= statetab && st < statetab + totthread));
	if(st && WAITREPLY(st) && amp->ah_tid == st->st_mytid && 
	   ADR_EQUAL(sa, &st->st_saddr)) { 
	    MON_EVENT("gotreceived: did NOT get reply");
	    STINC(rpc_snak);
	    sendamh(proc, sa, amp->ah_from, st->st_index, RPC_NAK, amp->ah_tid,
		    (adr_p) &amp->ah_kid);
	} else {
	    MON_EVENT("gotreceived: got reply");
	    STINC(rpc_sack);
	    sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_ACK, amp->ah_tid,
		    (adr_p) &amp->ah_kid);
	}
    } else if(amp->ah_flags & FL_REQUEST) {
	if((st = find(sa, amp->ah_from, amp->ah_tid)) != 0) { 
	    /* the server is working on it */
	    MON_EVENT("gotreceived: did get request");
	    STINC(rpc_sack);
	    sendamh(proc, sa, amp->ah_from, st->st_index, RPC_ACK, amp->ah_tid,
		    (adr_p) &amp->ah_kid);
	} else {
	    kid = kid_lookup(&amp->ah_kid);
	    if(kid == 0 || kid_execute_silent(kid, amp->ah_tid)) {
		/* didn't receive the request */
		MON_EVENT("gotreceived: did NOT get request");
		STINC(rpc_snak);
		sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_NAK,
			amp->ah_tid, (adr_p) 0);  
	    } else {  
		/* no idea what happened with this request; abort rpc */
		MON_EVENT("gotreceived: don't know about request");
		STINC(rpc_sfail);
		sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_FAIL,
			amp->ah_tid, (adr_p) 0);
	    } 
	}
    }
}


/* Receive forward; finish this rpc by sending an ack to the server.
 * Wake thread up, so it can send the request to another server in the
 * same group.
 */
static void gotforward(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    amphdr_p amp, ramp;
    state_p st;
    adr_p newaddr;
    int r;
#ifdef UNIX_STREAMS
    int saved_state;
#endif

    proto_cur_header(amp, pkt, amphdr_t);
    st = RPC_STATE(proc, amp->ah_dest);
    assert(st == 0 || (st >= statetab && st < statetab + totthread));
    if(st && WAITREPLY(st) && amp->ah_tid == st->st_mytid &&
       ADR_EQUAL(&st->st_saddr, sa)) { 
	proto_remove_header(pkt);
	PROTO_LOOK_HEADER(newaddr, pkt, adr_t);
	port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
	port_install(st->st_sident, newaddr, &st->st_sport, P_SOMEWHERE,
		     st->st_srtime, (char *) 0);
	st->st_saddr = *newaddr;
	st->st_timeout = -1;
	STINC(rpc_sack);
	assert(st->st_ackpkt);
	proto_cur_header(ramp, st->st_ackpkt, amphdr_t);
	ramp->ah_dest = amp->ah_from;
	rpc_setendian(ramp);
	proto_fix_header(st->st_ackpkt);
	r = flip_unicast(proc->ps_ifno, st->st_ackpkt, st->st_security, sa,
		     st->st_proc->ps_ep, (f_size_t) sizeof(amphdr_t),
			 st->st_ltime);
	if(r < 0) {
	    DPRINTF(0, ("gotforward: flip_unicast -> %d\n", r));
	    pkt_discard(st->st_ackpkt);
	}
	st->st_ackpkt = 0;
#ifdef UNIX_STREAMS
	saved_state = st->st_state;
#endif
	MAKE_IDLE(st);
	st->st_reassemble = -1;
	st->st_flag &= ~(F_REASSEM | F_RETRANS | F_SIGNAL);
	st->st_flag |= F_FORWARD;
#ifdef UNIX_STREAMS
	switch (saved_state) {
	    case PUTREQ:
		wakeup_putreq(st);
		break;
	    case GETREP:
		wakeup_getrep(st);
		break;
	    default:
		printf("gotforward: stange state %d\n", saved_state);
	}
#else
	wakeup((event) &st->st_state);
#endif
    } else {
	MON_EVENT("gotforward: old forward");
	STINC(rpc_sack);
	sendamh(proc, sa, amp->ah_from, amp->ah_dest, RPC_ACK, amp->ah_tid,
								    (adr_p) 0);
    }
}

static void gotfail(proc, pkt, sa)
    procstate_p proc;
    pkt_p pkt;
    adr_p sa;
{
    amphdr_p amp;
    state_p st;
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
#endif /* UNIX_STREAMS */
    
    proto_cur_header(amp, pkt, amphdr_t);
    assert(amp);
    st = RPC_STATE(proc, amp->ah_dest);
    assert(st == 0 || (st >= statetab && st < statetab + totthread));
#ifdef RPC_DEBUG
    console_enable(0);
    printf("gotfail %d from ", amp->ah_tid);
    print_adr_loc(sa);
    console_enable(1);
#endif /* RPC_DEBUG */
    if(st && WAITREPLY(st) && amp->ah_tid == st->st_mytid &&
       ADR_EQUAL(&st->st_saddr, sa)) { 
	MON_EVENT("gotfail: fail message");
	MAKE_IDLE(st);
	st->st_flag &= ~(F_REASSEM | F_SIGNAL | F_RETRANS | F_NACKED |
			 F_UNREACHABLE);
	st->st_rcnt = RPC_FAILURE;
	st->st_reassemble = -1;
#ifdef UNIX_STREAMS
	rpcfd = &rpc_dev[st - statetab];
	st->st_rhdr = 0;
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
    } else {
#ifdef RPC_DEBUG
	if (st != 0) {
	    DPRINTF(0, ("gotfail: ignored (ctid = %d, mytid %d, state %d)\n",
			amp->ah_tid, st->st_mytid, st->st_state));
	} else {
	    DPRINTF(0, ("gotfail: ignored (no rpc state)\n"))
	}
#endif /* RPC_DEBUG */
    }
}


/* A rpc protocol message arrived. Do a switch on the type of the message. */
static void rpc_switch(proc, pkt, sa, complete, messid, length, flag)
    procstate_p proc;
    register pkt_p pkt;
    register adr_p sa;
    f_size_t length;
    int complete;
    f_msgcnt_t messid;
    int flag;
{
    register amphdr_p amp;
    int bigendian;

    PROTO_LOOK_HEADER(amp, pkt, amphdr_t);
    if(amp == 0) {
	MON_EVENT("rpc_switch: no amoeba protocol header");
	return;
    }
    bigendian = amp->ah_flags & FL_BIGENDIAN;
    rpc_orderhdr(amp, bigendian);
    if(amp->ah_type != RPC_REQUEST && amp->ah_dest >= nthread_proc) {
	DPRINTF(0, ("rpc_switch: weird packet\n"));
	return;
    }
    END_MEASURE(rpc_rcv);
    switch(amp->ah_type) {
    case RPC_REQUEST:
	STINC(rpc_rrequest);
	BEGIN_MEASURE(rpc_end_getreq);
	gotrequest(proc, pkt, sa, length, complete, messid, bigendian, flag);
	break;
    case RPC_REPLY:
	STINC(rpc_rreply);
	BEGIN_MEASURE(rpc_ack);
	gotreply(proc, pkt, sa, length, complete, messid, bigendian, flag);
	break;
    case RPC_ACK:
	STINC(rpc_rack);
	BEGIN_MEASURE(rpc_rcv_putrep);
	gotack(proc, pkt, sa);
	break;
    case RPC_LOCATE:
	STINC(rpc_rlocate);
	gotlocate(proc, pkt, sa);
	break;
    case RPC_HEREIS:
	STINC(rpc_rhereis);
	gothereis(proc, pkt, sa);
	break;
    case RPC_NAK:
	STINC(rpc_rnak);
	gotnack(proc, pkt, sa);
	break;
    case RPC_ENQUIRE:	
	STINC(rpc_renquire);
	gotenquire(proc, pkt, sa);
	break;
    case RPC_ALIVE:
	STINC(rpc_ralive);
	gotalive(proc, pkt, sa);
	break;
    case RPC_RECEIVED:
	STINC(rpc_rreceived);
	gotreceived(proc, pkt, sa);
	break;
    case RPC_FORWARD:
	STINC(rpc_rforward);
	gotforward(proc, pkt, sa);
	break;
    case RPC_FAIL:
	STINC(rpc_rfail);
	gotfail(proc, pkt, sa);
	break;
    default:
	MON_EVENT("rpc_switch: unknown type");
	break;
    }
}


static void reassemble(st, pkt, length, total)
    state_p st;
    pkt_p pkt;
    f_size_t length, total;
{
    kid_p kid;
    f_size_t l, t;
#if !defined(UNIX) || defined(UNIX_STREAMS)
    f_size_t dirsize, virsize;
#endif /* !UNIX || UNIX_STREAMS */
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
#endif /* UNIX_STREAMS */
    
    l = (st->st_rcnt > st->st_roffset) ? MIN(st->st_rcnt - st->st_roffset,
					     length) : 0; 
    assert(l <= pkt->p_contents.pc_totsize);
    if(l > 0) {
#if !defined(UNIX) || defined(UNIX_STREAMS)
	dirsize = MIN(l, pkt->p_contents.pc_dirsize);
	virsize = l - dirsize;
	if(dirsize > 0) {
	    assert(st->st_rdata);
	    (void) memmove((_VOIDSTAR) (st->st_rdata + st->st_roffset),
			(_VOIDSTAR) pkt_offset(pkt), (size_t) dirsize);
	}
	if(virsize > 0) {
	    assert(st->st_rdata);
	    (void) memmove(
		    (_VOIDSTAR) (st->st_rdata + st->st_roffset + dirsize),
		    (_VOIDSTAR) pkt->p_contents.pc_virtual, (size_t) virsize);
	}
#else /* !UNIX || UNIX_STREAMS */
        assert(!(pkt->p_contents.pc_virtual != 0 && 
		pkt->p_contents.pc_dsident == 1 /* FL_DS_USER */ ));
	uxrpc_buffer(st->st_thread, pkt, st->st_rdata + st->st_roffset, l);
	pkt_keep = 1;
#endif /* !UNIX || UNIX_STREAMS */
    }
    t = total - sizeof(header) - sizeof(amphdr_t);
    st->st_roffset += length;
    if(WAITREPLY(st) && t == st->st_roffset) {
	MAKE_IDLE(st);
	st->st_flag &= ~(F_REASSEM | F_SIGNAL| F_RETRANS | F_NACKED);
	st->st_reassemble = -1;
	st->st_rcnt = MIN(st->st_rcnt, t);
	STINC(rpc_sack);
	sendamh(st->st_proc, &st->st_saddr, st->st_sident, st->st_index,
		RPC_ACK, st->st_mytid, (adr_p) 0); 
#ifdef UNIX_STREAMS
	st->st_rhdr = 0;
#ifdef STATISTICS
	if (st->st_rcnt < 64*1024)
	    INCSIZEBIN(rpc_grqbin, st->st_rcnt);
#endif
	END_MEASURE(rpc_getreq_reassem);
	BEGIN_MEASURE(rpc_doop);
	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_hdr2 = rpcfd->rpc_rhdr;
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	mp->b_cont->b_rptr = DB_BASE(mp->b_cont);
	mp->b_cont->b_wptr = mp->b_cont->b_rptr + st->st_rcnt;
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
	END_MEASURE(rpc_trans_reassem);
#endif /* UNIX_STREAMS */
	BEGIN_MEASURE(rpc_rcv_trans);
    } else if(st->st_state == GETREQ && t == st->st_roffset) {
	st->st_state = IDLE;
	st->st_flag &= ~F_REASSEM;
	st->st_reassemble = -1;
	st->st_flag |= F_DOOP;
	kid = kid_lookup(&st->st_ckid);
	if(kid == 0) kid = kid_install(&st->st_ckid, st->st_ctid);
	else kid_update(kid, st->st_ctid);
	st->st_rcnt = MIN(st->st_rcnt, t);
	STINC(rpc_sack);
	sendamh(st->st_proc, &st->st_caddr, st->st_cident, st->st_index,
		RPC_ACK, st->st_ctid, (adr_p) 0); 
#ifdef UNIX_STREAMS
	st->st_rhdr = 0;
#ifdef STATISTICS
	if (st->st_rcnt < 64*1024)
	    INCSIZEBIN(rpc_grqbin, st->st_rcnt);
#endif
	END_MEASURE(rpc_getreq_reassem);
	BEGIN_MEASURE(rpc_doop);
	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_hdr1 = rpcfd->rpc_rhdr;
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
	END_MEASURE(rpc_getreq_reassem);
#endif /* UNIX_STREAMS */
    }
    rpc_settimer((long) st); 	/* reset timer */
}


/* Offset in user data. */
#define PKTOFFSET(o)	(o + sizeof(header) + sizeof(amphdr_t))

/* A not-first fragment of a flip message arrived. We add the fragment to
 * the incomplete message that has been received by one of the threads.
*/
static void rpc_unidata(proc, pkt, src, messid, offset, length, total)
    procstate_p proc;
    register pkt_p pkt;
    adr_p src;
    f_msgcnt_t messid;
    f_size_t offset, length, total;
{
    register state_p st, *t, lastst;
    adr_p from;

    lastst = proc->ps_lastst;
    if(lastst != 0 && (lastst->st_flag & F_REASSEM)) { 
	if(WAITREPLY(lastst)) from = &lastst->st_saddr; 
        else from = &lastst->st_caddr;
	assert(REASSEMBLE(lastst));
	if(ADR_EQUAL(from, src) && lastst->st_rmessid == messid &&
	   PKTOFFSET(lastst->st_roffset) == offset) {
	    END_MEASURE(rpc_rcv);
	    reassemble(lastst, pkt, length, total);
	    return;
	}
    }
    for(t = proc->ps_thread; t < proc->ps_thread + nthread_proc; t++) {
	st = *t;
	if(st == 0) continue;
	if(!(st->st_flag & F_REASSEM)) continue;
	if(WAITREPLY(st)) from = &st->st_saddr;
        else from = &st->st_caddr;
	assert(REASSEMBLE(st));
	if (ADR_EQUAL(from, src) && st->st_rmessid == messid) {
	    END_MEASURE(rpc_rcv);
	    if (PKTOFFSET(st->st_roffset) == offset) {
		proc->ps_lastst = st;
		reassemble(st, pkt, length, total);
	    } else {
		DPRINTF(0, ("rpc_unidata: wrong pkt: mess %d; off (%d, %d)\n",
			messid, PKTOFFSET(st->st_roffset), offset));
		/* Since this packet arrived out of sequence, there is no
		 * point in waiting for the rest.  We just let the reassembly
		 * timer start a new rpc_getreq in the next run.  Note that
		 * we expect the network to be very reliable; if packet loss
		 * is happening regularly, some sort of sliding window protocol
		 * would be more efficient.
		 */
		st->st_reassemble = 1; /* times out in next run */
	    }
	    return;
	}
    }
}


/* A flip packet arrived. Don't accept messages addressed to a flip address
 * that is not listened to. 
 */
static void
rpc_receive(proc, pkt, dst, src, messid, offset, length, total, flag)
    procstate_p proc;
    pkt_p pkt;
    adr_p dst, src;
    f_msgcnt_t messid;
    f_size_t offset, length, total;
    int flag;
{
    long *protocol;

    BEGIN_MEASURE(rpc_rcv);
#ifdef UNIX
    pkt_keep = 0;
#endif
    assert(proc >= proctab && proc < proctab + nproc);
    if(ADR_NULL(dst)) {		/* broadcast? */
	PROTO_LOOK_HEADER(protocol, pkt, long);
	if(protocol == 0 || offset != 0)  {
	    pkt_discard(pkt);
	    return;
	}
	dec_l_be(protocol);	/* protocol field is sent in network order */
	if(*protocol != FLIP_AMRPC) {	
	    pkt_discard(pkt);
	    return;
	}
	proto_remove_header(pkt);
	total -= sizeof(long);
	length -= sizeof(long);
    }
    if(offset == 0) {	/* first fragment? */
	rpc_switch(proc, pkt, src, total == length, messid, length, flag);
    } else {
	rpc_unidata(proc, pkt, src, messid, offset, length, total);
    }
#ifdef UNIX
    if(!pkt_keep)
#endif
    pkt_discard(pkt);
}


/* A FLIP packet returns; the path to the destination is unknown or the
 * destination is dead. If fl is zero, the flip interface could not locate
 * the destination and we can assume that the destination is dead (we
 * don't have process migration yet).
 * If FLIP_NOTHERE is set in fl, we could try to send the packet again.
 * However, we let the thread who sent the message in the first place do the
 * work (rpc_notdeliver is probably called from the interrupt routine).
 */

/*ARGSUSED*/
static void
rpc_notdeliver(proc, pkt, dst, messid, offset, length, total, fl)
    procstate_p proc;
    pkt_p pkt;
    adr_p dst;
    f_msgcnt_t messid;
    f_size_t offset, length, total;
    int fl;
{
    amphdr_p amh;
    state_p st, *t;
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
#endif /* UNIX_STREAMS */

    assert(pkt);
    if(offset > 0) {
	pkt_discard(pkt);
	return;
    }
    PROTO_LOOK_HEADER(amh, pkt, amphdr_t);
    if(amh == 0) {
	pkt_discard(pkt);
	return;
    }
    if(amh->ah_from >= totthread) {
	DPRINTF(0, ("rpc_notdeliver: weird packet\n"));
	pkt_discard(pkt);
	return;
    }
    assert(proc);
    assert(proc >= proctab && proc < proctab + nproc);
    for(t = proc->ps_thread; t < proc->ps_thread + nthread_proc; t++) {
	st = *t;
	if(st != 0) {
	    if((st->st_state == PUTREP && ADR_EQUAL(dst, &st->st_caddr)) || 
	       (st->st_state == GETREP && ADR_EQUAL(dst, &st->st_saddr))) {
		MON_EVENT("rpc_notdeliver: could not reach server or client");
		if(fl == 0) {  /* the flip interface could not locate dst */
		    st->st_retrial = -1;
		}
#ifdef UNIX_STREAMS
		port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
		MAKE_IDLE(st);
		st->st_flag = 0;
		st->st_rhdr = 0;
		rpcfd = &rpc_dev[st - statetab];
		mp = rpcfd->mp;
		freemsg(rpcfd->mp_data);
		rpcfd->mp_data = 0;
		rpc_args = (struct rpc_args *) DB_BASE(mp);
		rpc_args->rpc_status = RPC_NOTFOUND;
		mp->b_rptr = DB_BASE(mp);
		mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
		qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
		wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
	    } else if(st->st_state == PUTREQ && ADR_EQUAL(dst, &st->st_saddr)) {
		DPRINTF(0, ("rpc_notdeliver: could not send request 0x%x\n", 
			    fl));
		if(fl & FLIP_UNTRUSTED) st->st_flag |= F_UNTRUSTED;
		if(!(st->st_flag & F_RETRANS)) {
		    st->st_flag |= F_UNREACHABLE;
		    st->st_timeout = -1;
		}
#ifdef UNIX_STREAMS
		port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
		wakeup_putreq(st);
#else /* UNIX_STREAMS */
		wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
	    }
	}
    }

    if(pkt->p_admin.pa_release == rpc_settimer)
	pkt->p_admin.pa_release = 0;	/* don't set timers */

    pkt_discard(pkt);
}


/* Allocate and fill an entry for the thread st in proc p. */
static int findindex(p, st)
    procstate_p p;
    state_p st;
{
    state_p *t;

    for(t = p->ps_thread; t < p->ps_thread + nthread_proc; t++) {
	if(*t == 0) {
	    *t = st;
	    st->st_index = t - p->ps_thread;
	    st->st_proc = p;
	    p->ps_nthread++;
	    return(1);
	}
    }
    return(0);
}


/* A thread is started. Initializes the transaction state for it.
 * Threads in the same process share the flip address.
 */

void rpc_initstate(t)
    thread_t *t;
{
    state_p st;
    procstate_p newp;
 
    st = RPC_THREADSTATE(t);
    assert(st >= statetab && st < statetab + totthread);
    st->st_ltime = LOCTIMEOUT * (LOCRETRIAL + 1) * (max_hops + 1);
    st->st_srtime = 0;
    st->st_crtime = 0;
    MAKE_IDLE(st);
    st->st_flag = 0;
    st->st_timeout = -1;
    st->st_mytid = 0;
    st->st_thread = t;
    st->st_reassemble = -1;
    st->st_ackpkt = 0;
    newp = &proctab[getpid(t)];
    assert(newp >= proctab && newp < proctab + nproc);
    if(newp->ps_init) { /* there is already a thread in this process */
	if(!findindex(newp, st)) {
	    panic("rpc_initstate: could not find free thread entry (a)\n");
	}
	return;
    }
    /* this is the first thread of this process */
    newp->ps_init = 1;
    newp->ps_lastst = 0;
    newp->ps_pid = getpid(t);
    newp->ps_ifno = flip_init((long) newp, rpc_receive, rpc_notdeliver);
    assert(newp->ps_ifno >= 0);
    adr_random(&newp->ps_myprvaddr);
    flip_oneway(&newp->ps_myprvaddr, &newp->ps_myaddr);
    if((newp->ps_ep = flip_register(newp->ps_ifno, &newp->ps_myprvaddr)) < 0)
	panic("rpc_initstate: flip_register failed");
    if((newp->ps_bcastep = flip_register(newp->ps_ifno, &nulladdr)) < 0)
	panic("rpc_initstate: flip_register failed");
    if(!findindex(newp, st))
	panic("rpc_initstate: could not find free thread entry (b)\n");
}


/* Clear state entry for a dying thread. This function is called from the
 * process management module when the process state is also removed.
 */
void rpc_cleanstate(t)
    thread_t *t;
{
    state_p me = RPC_THREADSTATE(t);
    procstate_p p;
    
    assert(me >= statetab && me < statetab + totthread);
    if(me->st_proc == 0) {  /* is thread alive? */
	DPRINTF(0, ("rpc_cleanstate: process is already gone\n"));
	return;
    }
    rpc_stoprpc(t);
    if(RPC_ACTIVE(me)) {
	DPRINTF(0, ("rpc_cleanstate: process still has rpc flags set: %d(0x%x)\n",
	       me->st_state, me->st_flag));
    }
    me->st_reassemble = -1;
    MAKE_IDLE(me);
    me->st_flag = 0;
    
    if(me->st_ackpkt) pkt_discard(me->st_ackpkt);
    p = me->st_proc;
    me->st_proc = 0;
    p->ps_thread[me->st_index] = 0;
    if(--p->ps_nthread > 0) return; /* not last thread? */
    if(flip_unregister(p->ps_ifno, p->ps_ep) < 0)
	panic("rpc_cleanstate: flip_unregister failed");
    if(flip_unregister(p->ps_ifno, p->ps_bcastep) < 0)
	panic("rpc_cleanstate: flip_unregister bcastep failed");
    if(flip_end(p->ps_ifno) < 0)
	panic("rpc_cleanstate: flip_close failed");
    p->ps_init = 0;
}


#ifndef UNIX
/* This routine is called from other part of the kernels to find out
 * the flip address of this process.
 */
void rpc_myaddr(addr)
    adr_p addr;
{
    state_p st;
    thread_t *t = curthread;

    st = RPC_THREADSTATE(t);
    assert(st >= statetab && st < statetab + totthread);
    *addr = st->st_proc->ps_myaddr;
}
#endif  /* UNIX */

void remove_waiting();

int rpc_sendsig(t)
    thread_t *t;
{
    state_p st = RPC_THREADSTATE(t);
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
#endif /* UNIX_STREAMS */

    assert(st >= statetab && st < statetab + totthread);

    if(!RPC_ACTIVE(st)) { /* busy doing a RPC? */
	return(0);
    }
    switch(st->st_state) {
    case LOCATE:
	MAKE_IDLE(st);
	st->st_flag |= F_SIGNAL;
#ifdef UNIX_STREAMS
	MON_EVENT("findserver: locate terminated due to signal");
	assert(st->st_state == IDLE);
	assert(st->st_timeout < 0);
	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = RPC_NOTFOUND;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
	break;
    case GETREQ:
	MAKE_IDLE(st);
	st->st_flag = 0;
	st->st_reassemble = -1;
	port_remove(&st->st_pubport, &st->st_proc->ps_myaddr, st->st_index, 1);
#if defined(UNIX) && !defined(UNIX_STREAMS)
	uxrpc_cleanup(st->st_thread);
#endif /* UNIX && !UNIX_STREAMS */
	st->st_rcnt = RPC_ABORTED;
#ifdef UNIX_STREAMS
	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
	break;
    case GETREP:
	st->st_flag |= F_SIGNAL;
	sendenquire(st, 0);
	break;
    case PUTREQ:
	st->st_flag |= F_SIGNAL;
	break;
    }
    
    if(DOOPERATION(st))
	return(0);
    return(1);
}


/* RPC is forced to terminate. */
void rpc_stoprpc(t)
    thread_t *t;
{
    void remove_waiting();
    state_p st = RPC_THREADSTATE(t);
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
#endif /* UNIX_STREAMS */
    
    assert(st >= statetab && st < statetab + totthread);
    assert(st->st_proc);
    
    DPRINTF(1, ("rpc_stoprpc: index %d, pid %d\n",
				st->st_index, st->st_proc->ps_pid));
    (void) rpc_sendsig(t);
    if(!RPC_ACTIVE(st)) {
	return;
    }
    if(st->st_state == LOCATE || st->st_state == IDLE) {
	st->st_flag &= ~F_SIGNAL;
    }
    if(WAITREPLY(st)) {
	MAKE_IDLE(st);
	st->st_flag &= ~(F_SIGNAL|F_REASSEM| F_RETRANS | F_NACKED | 
			 F_UNREACHABLE);
	st->st_rcnt = RPC_ABORTED;
	st->st_reassemble = -1;
#ifdef UNIX_STREAMS
	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
#else /* UNIX_STREAMS */
	wakeup((event)&st->st_state);
#endif /* UNIX_STREAMS */
    } 
    if(SERVING(st)) {
	DPRINTF(0, ("rpc_stoprpc: pid %d was serving\n", st->st_proc->ps_pid));
	if(st->st_flag & F_DOOP) {
	    STINC(rpc_sfail);
	    assert(st->st_proc);
	    sendamh(st->st_proc, &st->st_caddr, st->st_cident, st->st_index,
		    RPC_FAIL, st->st_ctid, (adr_p) 0);
	}
	st->st_flag &= ~(F_DOOP | F_NACKED | F_SIGTRANS);
	MAKE_IDLE(st);
    }
    assert(!RPC_ACTIVE(st));
}


#ifdef UNIX_STREAMS

void wakeup_putreq(state_p st)
{
    uint8 amflags = 0;		/* amoeba header flags */
    int flflags = 0;		/* flip header flags */
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;

    rpcfd = &rpc_dev[st - statetab];
    if (st->st_flag & F_FORWARD) {
	st->st_flag &= ~F_FORWARD;
	st->st_mytid = mytsn++;
	st->st_retrial = MAX_RPC_RETRIAL;
	assert(!(st->st_flag & F_FORWARD));
	if(!st->st_cache || !(PORTCMP(&st->st_cache->p_pubport,
				    &st->st_sport))) { 
	    rpcfd = &rpc_dev[st - statetab];
	    rpcfd->lochcnt = 1;
	    rpcfd->loctime = 0;
	    rpcfd->maxretrial = LOCRETRIAL;
	    if (findserver(st, &st->st_sport, rpcfd->lochcnt))
		deliverreq(st, 0, 0);
	} else {
	    deliverreq(st, 0, 0);
	}
	return;
    }
    if (st->st_retrial-- <= 0)  {  /* give up? */
	MON_EVENT("wakeup_putreq: server crashed");
	st->st_rcnt = (st->st_flag & F_NACKED || st->st_flag &
		       F_UNREACHABLE) ? RPC_NOTFOUND : RPC_FAILURE;
	MAKE_IDLE(st);
	st->st_flag &= ~(F_SIGNAL | F_RETRANS | F_UNREACHABLE | 
			 F_NACKED | F_UNTRUSTED);
	port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
	st->st_rhdr = 0;
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
	return;
    }
    if (st->st_flag & F_UNREACHABLE)
	flflags |= FLIP_INVAL;
    if (!(st->st_flag & (F_NACKED | F_UNREACHABLE)) &&
		!(flflags & FLIP_INVAL)) {
#ifdef RPC_DEBUG
	DPRINTF(0, ("deliverreq: send received %d\n", st->st_mytid));
#else
	MON_EVENT("deliverreq: send received");
#endif
	/* Check if the server has received the request. Furthermore,
	 * remember that we have sent a request that possibly could
	 * have reached the server (F_RETRANS is set).
	 */
	st->st_flag |= F_RETRANS;
	sendreceived(st, &st->st_saddr, st->st_sident, st->st_index,
		st->st_mytid, FL_REQUEST);
	return;
    }

    /* At this point, we know that the server did not receive the request.
     * There are three possible reasons:
     * 1) the server is busy and has sent a NACK.
     * 2) the server did not get it and has sent a NACK.
     * 3) the message is returned by the FLIP box, because there is no
     *    route (a gateway forgot the route, the server migrated, the
     *    server is dead, route is not trusted).
     */
    /* Next time we sent the message, we have to set the retransmission
     * flag in the amoeba protocol header, if we did not get a response on
     * a request.
     */ 
    if(st->st_flag & F_RETRANS) amflags |= FL_RETRANS;

    /* If the server has responded on all requests, or the message is
     * returned by the FLIP box after locating the destination, then
     * do a RPC locate to find another server listening to the same port.
     */
    if(!(st->st_flag & F_RETRANS) && (st->st_retrial <= FIRSTLOCATE ||
				      flflags & FLIP_INVAL)) {
	DPRINTF(0, ("deliverreq: still not reachable: remove port %d\n",
		st->st_retrial));

	if(st->st_flag & F_UNTRUSTED) {
	    /* No safe path to server; encryption in needed. */
	    MON_EVENT("deliverreq: no safe path to server");
	    MAKE_IDLE(st);
	    st->st_flag &= ~(F_SIGNAL | F_UNTRUSTED | F_UNREACHABLE);
	    st->st_rcnt = RPC_UNSAFE;

	    rpcfd = &rpc_dev[st - statetab];
	    mp = rpcfd->mp;
	    st->st_rhdr = 0;
	    freemsg(rpcfd->mp_data);
	    rpcfd->mp_data = 0;
	    rpc_args = (struct rpc_args *) DB_BASE(mp);
	    rpc_args->rpc_status = st->st_rcnt;
	    mp->b_rptr = DB_BASE(mp);
	    mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	    qreply(rpcfd->queue, mp);
	    return;
	}
	st->st_flag &= ~(F_SIGNAL | F_UNREACHABLE | F_NACKED | F_UNTRUSTED);
	port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
	st->st_cache = 0;

	if(!findserver(st, &st->st_sport)) { 
	    /*
	    st->st_rcnt = (st->st_flag & F_SIGNAL) ? RPC_ABORTED :
		RPC_NOTFOUND; 
	    st->st_flag &= ~(F_SIGNAL | F_RETRANS);

	    rpcfd = &rpc_dev[st - statetab];
	    mp = rpcfd->mp;
	    st->st_rhdr = 0;
	    freemsg(rpcfd->mp_data);
	    rpcfd->mp_data = 0;
	    rpc_args = (struct rpc_args *) DB_BASE(mp);
	    rpc_args->rpc_status = st->st_rcnt;
	    mp->b_rptr = DB_BASE(mp);
	    mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	    qreply(rpcfd->queue, mp);
	    */
	    return;
	}
	assert(st->st_cache);
	st->st_srtime = st->st_cache->p_roundtrip;
	st->st_saddr = st->st_cache->p_addr;
	st->st_state = PUTREQ;
	/* st->st_retrial = MAX_RPC_RETRIAL; */
	flflags = 0;
    } else {
	/* Tell the FLIP-box to forget about its routing info. */
	if (st->st_flag & F_UNREACHABLE)
	    flflags |= FLIP_INVAL;
	st->st_flag &= ~(F_NACKED | F_UNREACHABLE | F_UNTRUSTED);
    }
    /* Send the request again */
    if (st->st_cache)
	deliverreq(st, amflags, flflags);
}


void wakeup_getrep(state_p st)
{
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;
    int f = 0;

    if (st->st_flag & F_FORWARD) {
	st->st_flag &= ~F_FORWARD;
	st->st_mytid = mytsn++;
	st->st_retrial = MAX_RPC_RETRIAL;
	assert(!(st->st_flag & F_FORWARD));
	/* A new address was installed in the port table when a forward
	** packet arrived. So a port_lookup() should just work.
	*/
	if ((st->st_cache = port_lookup(&st->st_sport)) == 0) {
	    printf("wakeup_getrep: no forward port installed\n");
	    return;
	}
	/* deliverreq() uses the information in st->st_cache */
	deliverreq(st, 0, 0);
	return;
    }
    if(st->st_retrial-- <= 0) {
	MON_EVENT("waitrep: server crashed");
	MAKE_IDLE(st);
	st->st_flag &= ~(F_SIGNAL | F_REASSEM);
	st->st_reassemble = -1;
	port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0); 
	st->st_rcnt = RPC_FAILURE;

	rpcfd = &rpc_dev[st - statetab];
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = st->st_rcnt;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
    }
    if(st->st_retrial <= FIRSTLOCATE) f |= FLIP_INVAL;
    sendenquire(st, f);
    f = 0;
    assert(st->st_state == GETREP);
}


static void wakeup_locate(state_p st)
{
    struct rpc_device *rpcfd;

    rpcfd = &rpc_dev[st - statetab];
    rpcfd->loctime += st->st_deltatime;
    if(--rpcfd->maxretrial == 0) {
	if(rpcfd->lochcnt < max_hops)
	    rpcfd->lochcnt = MIN((uint16)
		    (2 * rpcfd->lochcnt + 1), max_hops);
	else st->st_deltatime *= 2;
	rpcfd->maxretrial = LOCRETRIAL;
    }
    if (findserver(st, &st->st_sport, rpcfd->lochcnt))
	deliverreq(st, 0, 0);
}

#endif /* UNIX_STREAMS */


/* Sweep timers. */
static void
rpc_sweep(id)
    long id;
{
    register state_p st;

    if(!timerrunning) return;

    timerrunning = 0;
    for(st = statetab; st < statetab + totthread; st++) {
	if(!st->st_proc) continue;
	if(st->st_state == IDLE) {
	    assert(st->st_timeout < 0);
	    continue;
	}
	if(st->st_timeout > 0) {
	    st->st_timeout -= id;
	    if(st->st_timeout <= 0) {
		st->st_timeout = -1;
		STINC(rpc_timeout);
#ifdef UNIX_STREAMS
		switch (st->st_state) {
		    case PUTREQ:
			wakeup_putreq(st);
			break;
		    case GETREP:
		    case PUTREP:
			wakeup_putrep(st);
			break;
		    case LOCATE:
			wakeup_locate(st);
			break;
		    default:
			printf("rpc_sweep: timer ran out for unknown state\n");
		}
#else /* UNIX_STREAMS */
		wakeup((event) &st->st_state);
#endif /* UNIX_STREAMS */
	    } else timerrunning = 1;
	}
	if(st->st_reassemble > 0) {
	    st->st_reassemble -= id;
	    if(st->st_reassemble <= 0) {
	      DPRINTF(0, ("rpc_sweep: reassemble timer expires (mess %d) %d\n", 
					st->st_rmessid, st->st_roffset)); 
		assert(st->st_flag & F_REASSEM);
		st->st_reassemble = -1;
#if defined(UNIX) && !defined(UNIX_STREAMS)
		uxrpc_cleanup(st->st_thread);
#endif /* UNIX && ! UNIX_STREAMS */
		st->st_flag &= ~F_REASSEM;
		if(st->st_state == GETREQ) {
		    port_install(st->st_index, &st->st_proc->ps_myaddr,
			     &st->st_pubport, P_LOCAL,
			     (interval) SWEEPINTERVAL, (char *) st->st_proc);
		}
	    } else timerrunning = 1;
	}
    }
}


/* Initialize this module. */
void
rpc_init()
{
    state_p st, *t;
    procstate_p p;
    
    DPRINTF(0, ("rpc_init: aalloc %d thread(%d); %d proc(%d)\n", totthread,
	   sizeof(state_t), nproc, sizeof(procstate_t)));

    /* We are throwing away memory everywhere, here we continue this
     * tradition. 
     */
    
    nthread_proc = totthread;
    compare(sizeof(amphdr_t), ==, 24);
    statetab = (state_p) aalloc((vir_bytes)(sizeof(state_t) * totthread), 0);
    proctab = (procstate_p) aalloc((vir_bytes)(sizeof(procstate_t) * nproc), 0);
    adr_random(&mykid);
#ifdef DEBUG
    DPRINTF(0, ("rpc_init: kid  is "));
#if defined(PRINTF_LEVEL) && PRINTF_LEVEL >= 0
    adr_print(&mykid);
#endif
    DPRINTF(0, ("\n"));
#endif /* DEBUG */
    for(st = statetab; st < statetab + totthread; st++) {
	st->st_thread = 0;
	st->st_proc = (procstate_p) 0;
	st->st_reassemble = -1;
	st->st_flag = 0;
	MAKE_IDLE(st);
	st->st_security = FLIP_SECURITY;
    }
    for(p = proctab; p < proctab + nproc; p++) {
	p->ps_init = 0;
	p->ps_thread = (state_p *)
			aalloc((vir_bytes)(nthread_proc * sizeof(state_p)), 0);
	for(t = p->ps_thread; t < p->ps_thread + nthread_proc; t++) {
	    *t = (state_p) 0;
	}
    }
    DPRINTF(0, ("rpc_init: aalloc %d pkts of size %d\n", rpc_npkt, rpc_pktsize));
    rpc_pkt = (pkt_p) aalloc((vir_bytes)(sizeof(pkt_t) * rpc_npkt), 0);
    rpc_pktdata = aalloc((vir_bytes)(rpc_npkt * rpc_pktsize), 0);
    pkt_init(&rpc_pool, (int) rpc_pktsize, rpc_pkt, (int) rpc_npkt,
					    rpc_pktdata, (void (*)()) 0, 0L);
    sweeper_set(rpc_sweep, (long) SWEEPINTERVAL, (interval) SWEEPINTERVAL, 0);
#if !defined(NO_MPX_REGISTER) && !defined(UNIX)
    mpx_register(rpc_initstate, rpc_stoprpc, rpc_sendsig, rpc_cleanstate);
#endif
    rpc_initialised = 1;
}


/*
 * rpc user interface
 */


#ifndef UNIX

/* Set timeout values. */
/*ARGSUSED*/
interval rpc_timeout(ltime, stime)
    interval ltime, stime;
{
    thread_t *t = (thread_t *) curthread;
    state_p st;
    interval oldval;
    
    st = RPC_THREADSTATE(t);
    assert(st >= statetab && st < statetab + totthread);
    oldval = st->st_ltime;
    st->st_ltime = ltime;
    return (interval) oldval;
}


/*
 * Some leftover support routines.
 * This definitely needs to be cleaned up
 */

interval
timeout(maxloc)
interval maxloc;
{

    return rpc_timeout(maxloc, (interval) 0);
}

long tgtimeout(tp)
thread_t *tp;
{
    state_p st;

    st = RPC_THREADSTATE(tp);
    assert(st >= statetab && st < statetab + totthread);
    return st->st_ltime;
}

void tstimeout(tp, tout)
thread_t * tp;
long            tout;
{
    state_p st;

    st = RPC_THREADSTATE(tp);
    assert(st >= statetab && st < statetab + totthread);
    st->st_ltime = tout;
}

#endif /* UNIX */


/* A thread is ready to accept a request. */
#ifdef SECURITY
long secure_getreq(hdr, buf, cnt)
#else
long rpc_getreq(hdr, buf, cnt)
#endif
    header *hdr;
    bufptr buf;
    f_size_t cnt;
{
    thread_t *t = (thread_t *) curthread;
    register state_p st;
    
    BEGIN_MEASURE(rpc_start_getreq);
    if(!hdr) {
	return(RPC_BADADDRESS);
    }
    if(NULLPORT(&hdr->h_port)) {
	return(RPC_BADPORT);
    }
    st = RPC_THREADSTATE(t);
    assert(st >= statetab && st < statetab + totthread);
    assert(st->st_proc);
    if(RPC_ACTIVE(st)) {
	progerror();
	return(RPC_FAILURE);
    }
    if(cnt > 0 && buf == 0) {
	progerror();
	return(RPC_BADADDRESS);
    }
    
    assert(st->st_rhdr == 0);
    st->st_rdata = buf;
    st->st_rcnt = cnt;
    st->st_rhdr = hdr;
#if PRINTF_LEVEL >= 1
    bprintf(0, 0, "install port %P\n", &hdr->h_port);
#endif
    priv2pub(&hdr->h_port, &st->st_pubport);
    port_install(st->st_index, &st->st_proc->ps_myaddr, &st->st_pubport,
		 P_LOCAL, (interval) SWEEPINTERVAL, (char *) st->st_proc);
    st->st_state = GETREQ;
    END_MEASURE(rpc_start_getreq);
#ifdef UNIX_STREAMS
    /*
     * We ferret out the return information (request size) at a higher
     * level in the stream.
     */
    return STD_OK;
#else /* UNIX_STREAMS */
    (void) await_reason((event) &st->st_state, 0L, "getreq");
    assert(DOOPERATION(st) || !RPC_ACTIVE(st));
    if (cnt > 0) {
	uunmap(t, (vir_bytes) buf, (vir_bytes) cnt);
    }
    assert(st->st_rhdr == hdr);
    st->st_rhdr = 0;
#ifdef STATISTICS
    if (st->st_rcnt < 64*1024)
	INCSIZEBIN(rpc_grqbin, st->st_rcnt);
#endif
    END_MEASURE(rpc_end_getreq);
    BEGIN_MEASURE(rpc_doop);
    return(st->st_rcnt);
#endif /* UNIX_STREAMS */
}


/* Send the request */
#ifdef __STDC__
static void sendreq(state_p st, uint8 amflags, int flflags)
#else /* __STDC__ */
static void sendreq(st, amflags, flflags)
    state_p st;
    uint8 amflags;
    int flflags;
#endif /* __STDC__ */
{
    register pkt_p msg;
    int r;
    
    PKT_GET(msg, &rpc_pool);
    flflags |= st->st_security;
    if(msg == 0) {
	DPRINTF(0, ("WARNING: sendreq: out of packets"));
    } else {
	proto_init(msg);
	sethdr(msg, st->st_shdr);
	setamp(msg, (uint16) st->st_srtime, st->st_index, RPC_REQUEST, amflags,
	       st->st_mytid, (adr_p) &mykid, &st->st_sport);
	if(st->st_scnt > 0) {
#ifdef UNIX
#ifdef UNIX_STREAMS
	    proto_set_virtual(msg, 0, (long) st->st_thread, (long) 
			      st->st_sdata, st->st_scnt); 
#else  /* UNIX_STREAMS */
	    proto_set_virtual(msg, FL_DS_USER, (long) st->st_thread, (long) 
			      st->st_sdata, st->st_scnt); 
#endif  /* UNIX_STREAMS */
#else
	    proto_set_virtual(msg, 0, 0, (long) st->st_sdata, st->st_scnt); 
#endif
	}
	STINC(rpc_srequest);
	pkt_set_release(msg, rpc_settimer, (long) st);
/* #endif  /* UNIX_STREAMS */
	END_MEASURE(rpc_snd_trans);
#ifdef UNIX
	r = flip_unicast(st->st_proc->ps_ifno, msg, FLIP_SYNC | flflags,
		     &st->st_saddr, st->st_proc->ps_ep, 
		     sizeof(header) + sizeof(amphdr_t) + st->st_scnt,
		     st->st_ltime);
	if(r == FLIP_UNSAFE) st->st_flag |= F_UNTRUSTED;
	if(r < 0 && !(st->st_flag & F_RETRANS)) {
	    st->st_flag |= F_UNREACHABLE;
	}
#else
	r = flip_unicast(st->st_proc->ps_ifno, msg, flflags, &st->st_saddr,
		     st->st_proc->ps_ep, sizeof(header) + sizeof(amphdr_t) +
		     st->st_scnt, st->st_ltime); 
#endif
	if(r < 0) {
 	    DPRINTF(0, ("sendreq: flip_unicast -> %d\n", r));
	    pkt_discard(msg);
	}
    }
}


/* 
 * This following piece of code will disappear hopefully soon (when the
 * amoeba 4.0 RPCs are gone competely. It switches dynamically between
 * FLIP RPC and amoeba 4.0 RPC (depending in which portcache the port in
 * the header appears. If the port is unknown it first tries amoeba 4.0 and
 * then FLIP (if amoeba4.0 returns RPC_NOTFOUND); other the other way around
 * (depending on the value of FLIP_RPC_FIRST).
 */
#ifndef UNIX
#if defined(__STDC__)
bufsize trans(header *hdr1, bufptr buf1, _bufsize cnt1,
	      header *hdr2, bufptr buf2, _bufsize cnt2)
#else /* __STDC__ */
bufsize trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
    header *hdr1, *hdr2;
    bufptr buf1, buf2;
    bufsize cnt1, cnt2;
#endif /* __STDC__ */
{
    if (cnt1 > 30000 || cnt2 > 30000) {
	progerror();
	return RPC_FAILURE;
    }
    return (bufsize) rpc_trans(hdr1, buf1, (f_size_t) cnt1,
				hdr2, buf2, (f_size_t) cnt2);
}


#if defined(__STDC__)
bufsize getreq(header *hdr1, bufptr buf1, _bufsize cnt1)
#else /* __STDC__ */
bufsize getreq(hdr1, buf1, cnt1)
    header *hdr1;
    bufptr buf1;
    bufsize cnt1;
#endif /* __STDC__ */
{
    if (cnt1 > 30000) {
	progerror();
	return RPC_FAILURE;
    }
    return (bufsize) rpc_getreq(hdr1, buf1, (f_size_t) cnt1);
}


#if defined(__STDC__)
void putrep(header *hdr1, bufptr buf1, _bufsize cnt1)
#else /* __STDC__ */
void putrep(hdr1, buf1, cnt1)
    header *hdr1;
    bufptr buf1;
    bufsize cnt1;
#endif /* __STDC__ */
{
    if (cnt1 > 30000) {
	progerror();
	return;
    }
    (void) rpc_putrep(hdr1, buf1, (f_size_t) cnt1);
}


void cleanup()
{
    rpc_cleanup();
}
#endif /* UNIX */


/* Prepare a message. */
static void prepareclient(st, msg, dst, type, flags)
    state_p st;
    pkt_p *msg;
    uint16 dst;
    uint8 type;
    uint8 flags;
{
    register amphdr_p amh;
    
    PKT_GET(*msg, &rpc_pool);
    if(*msg == 0) {
	DPRINTF(0, ("WARNING: prepareclient: out of packets"));
    } else {
	proto_init(*msg);
	PROTO_ADD_HEADER(amh, *msg, amphdr_t);
	amh->ah_dest = dst;
	amh->ah_from = st->st_index;
	amh->ah_type = type;
	amh->ah_flags = flags;
	amh->ah_tid = st->st_mytid;
	(*msg)->p_admin.pa_priority = 1;
    }
}


/* The server is local; take the fast path. */
static void localreq(client_st)
    register state_p client_st;
{
    procstate_p proc = (procstate_p) (client_st->st_cache->p_arg);
    register state_p st;
    uint16 node;
    f_size_t l;

    if(!port_match(&client_st->st_sport, &proc->ps_myaddr, &node)) {
	panic("localreq: server is not here anymore");
    }
    STINC(rpc_localreq);
    st = RPC_STATE(proc, node);
    st->st_caddr = client_st->st_proc->ps_myaddr;
    st->st_ctid = client_st->st_mytid;
    st->st_cident = client_st->st_index;
    st->st_local_client = client_st->st_proc;
    *st->st_rhdr = *client_st->st_shdr;
#ifdef SECURITY
    st->st_rhdr->h_signature._portbytes[0] = 1;
#endif
    l = MIN(st->st_rcnt, client_st->st_scnt);
    if(l > 0) {
	assert(st->st_rdata);
	(void) memmove((_VOIDSTAR) st->st_rdata,
				(_VOIDSTAR) client_st->st_sdata, (size_t) l); 
    }
    st->st_flag |= F_DOOP;
    assert(!(st->st_flag & F_REASSEM));
    assert(st->st_timeout < 0);
    assert(st->st_reassemble < 0);
    MAKE_IDLE(st);
    st->st_rcnt = l;
    END_MEASURE(rpc_snd_trans);
    BEGIN_MEASURE(rpc_end_getreq);
    BEGIN_MEASURE(rpc_rcv_trans);
    wakeup((event) &st->st_state);
    client_st->st_state = GETREP;
    client_st->st_sident = st->st_index;
}


/* Check if the destination got the request or reply. */
#ifdef __STDC__
static void sendreceived(
    state_p st,
    adr_p da,
    uint16 dest,
    uint16 from,
    uint32 tid,
    uint8 am_flag)
#else /* __STDC__ */
static void sendreceived(st, da, dest, from, tid, am_flag)
    state_p st;
    adr_p da;
    uint16 dest, from;
    uint32 tid;
    uint8 am_flag;
#endif /* __STDC__ */
{
    pkt_p msg;
    int f = 0;
    int r;

    PKT_GET(msg, &rpc_pool);
    f |= st->st_security;
    if(msg == 0) {
	DPRINTF(0, ("WARNING: sendreceived: out of packets"));
    } else {
	proto_init(msg);
	if(st->st_retrial <= FIRSTLOCATE) f |= FLIP_INVAL;
	pkt_set_release(msg, rpc_settimer, (long) st);
	setamp(msg, dest, from, RPC_RECEIVED, am_flag, tid, (adr_p) &mykid,
	       (port *) 0); 
	STINC(rpc_sreceived);
	r = flip_unicast(st->st_proc->ps_ifno, msg, f, da,
			     st->st_proc->ps_ep, (f_size_t) sizeof(amphdr_t),
			     st->st_ltime);
	if (r < 0) {
	    DPRINTF(0, ("sendreceived: flip_unicast -> %d\n", r));
	    pkt_discard(msg);
	}
    }
}


#ifdef UNIX_STREAMS

/* Deliver the request to the server. */
#ifdef __STDC__
static void deliverreq(state_p st, uint8 amflags, int flflags)
#else /* __STDC__ */
static void deliverreq(st, amflags, flflags)
state_p st;
uint8 amflags;
int flflags;
#endif /* __STDC__ */
{
    /* If the pointer in the port cache is still valid and the ports are
     * the same, then continue. Otherwise, try to find the server.
     */

    /* The server is located; we know its FLIP address. */
    if (!st->st_cache)
	return;
    st->st_srtime = st->st_cache->p_roundtrip;
    st->st_saddr = st->st_cache->p_addr;
    st->st_state = PUTREQ;
    sendreq(st, amflags, flflags);
    if(st->st_ackpkt) pkt_discard(st->st_ackpkt);
    /* build next ack packet */
    prepareclient(st, &st->st_ackpkt, st->st_sident, RPC_ACK, 0);
}

#else /* UNIX_STREAMS */
/* Deliver the request to the server. */
static void deliverreq(st)
    state_p st;
{
    uint8 amflags = 0;		/* amoeba header flag */
    int flflags = 0;		/* flip header flag */
#ifdef RPC_DEBUG
    int nrequests = 0;
#endif /* RPC_DEBUG */

    /* If the pointer in the port cache is still valid and the ports are
     * the same, then continue. Otherwise, try to find the server.
     */
    if(!st->st_cache || !(PORTCMP(&st->st_cache->p_pubport, &st->st_sport))) { 
	if(!findserver(st, &st->st_sport)) { 
	    st->st_rcnt = (st->st_flag & F_SIGNAL) ? RPC_ABORTED :
		RPC_NOTFOUND; 
	    st->st_flag &= ~(F_SIGNAL | F_RETRANS);
	    return;
	}
    }
    /* The server is located; we know its FLIP address. */
    assert(st->st_cache);
    st->st_srtime = st->st_cache->p_roundtrip;
    st->st_saddr = st->st_cache->p_addr;
    st->st_state = PUTREQ;
#ifndef UNIX
    if(st->st_cache->p_where == P_LOCAL) { /* local rpc? */
	localreq(st);
	assert(NOCLIENT(st) || (st->st_state == GETREP) ||
		(st->st_flag & F_FORWARD));
	return;
    }
#endif /* UNIX */
    do {
	int timed_out;

	/* Send the request. */
	sendreq(st, amflags, flflags);
#ifdef RPC_DEBUG
	nrequests++;
	if (nrequests > 1) {
	    console_enable(0);
	    printf("retrans %d (%x, %x, %x) of %d",
		   nrequests, st->st_flag, amflags, flflags, st->st_mytid);
	    if (nrequests == 2) {
		printf(" to ");
		print_adr_loc(&st->st_saddr);
	    } else {
		printf("\n");
	    }
	    console_enable(1);
	}
#endif /* RPC_DEBUG */
	if(st->st_ackpkt) pkt_discard(st->st_ackpkt);
	/* build next ack packet */
	prepareclient(st, &st->st_ackpkt, st->st_sident, RPC_ACK, 0);
	do {
	    /* Wait until the server has sent a reply or ack (normal case),
             * or until a timer runs out or the message is returned by the
	     * FLIP-box.
	     */
	    if(st->st_state == PUTREQ)  {
		(void) await_reason((event) &st->st_state, 0L, "deliverreq");
	    }
	    if(st->st_state != PUTREQ) { /* done? */
		st->st_flag &= ~(F_RETRANS | F_NACKED | F_UNTRUSTED |
				 F_UNREACHABLE);
		if (! (NOCLIENT(st) || (st->st_state == GETREP)
		       || (st->st_flag & F_FORWARD)))
		{
			printf("deliverreq: strange state %d, flag = 0x%x\n",
				st->st_state, st->st_flag);
		}
		return;
	    }
	    /* A timer has run out, the message is returned by the FLIP 
	     * box, or the server has sent a NACK indicating that it did not
	     * receive the request or is not listening to the port.
	     */
	    assert(st->st_state == PUTREQ);
	    if(st->st_retrial-- <= 0)  {  /* give up? */
 		MON_EVENT("deliverreq: server crashed");
		st->st_rcnt = (st->st_flag & F_NACKED || st->st_flag &
			       F_UNREACHABLE) ? RPC_NOTFOUND : RPC_FAILURE;
		MAKE_IDLE(st);
		st->st_flag &= ~(F_SIGNAL | F_RETRANS | F_UNREACHABLE | 
				 F_NACKED | F_UNTRUSTED);
		port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
		return;
	    }

	    /* If the packet is returned by a FLIP box and we are sure we have
	     * not reached the server yet, either F_UNREACHABLE is set or
	     * FLIP_INVAL is set in flflags.  If the server sends a NACK back,
	     * F_NACKED is set.  If none of these flags is set, a timer has
	     * run out.
	     */
	    timed_out = (!(st->st_flag & (F_NACKED | F_UNREACHABLE)) &&
			 !(flflags & FLIP_INVAL));
	    if (timed_out) {
#ifdef RPC_DEBUG
		DPRINTF(0, ("deliverreq: send received %d\n", st->st_mytid));
#else /* RPC_DEBUG */
		MON_EVENT("deliverreq: send received");
#endif /* RPC_DEBUG */
		/* Check if the server has received the request. Furthermore,
		 * remember that we have sent a request that possibly could
		 * have reached the server (F_RETRANS is set).
		 */
		st->st_flag |= F_RETRANS;
		sendreceived(st, &st->st_saddr, st->st_sident, st->st_index,
			     st->st_mytid, FL_REQUEST);
	    }
	} while (timed_out);
#ifdef RPC_DEBUG
	DPRINTF(0, ("deliverreq: timed out waiting for ACK\n"));
#endif /* RPC_DEBUG */

	/* At this point, we know that the server did not receive the request.
	 * There are three possible reasons:
	 * 1) the server is busy and has sent a NACK.
	 * 2) the server did not get it and has sent a NACK.
	 * 3) the message is returned by the FLIP box, because there is no
	 *    route (a gateway forgot the route, the server migrated, the
	 *    server is dead, route is not trusted).
	 */
	/* Next time we send the message, we have to set the retransmission
	 * flag in the amoeba protocol header, if we did not get a response on
	 * a request.
	 */ 
	if(st->st_flag & F_RETRANS) amflags |= FL_RETRANS;

	/* If the server has responded on all requests, or the message is
	 * returned by the FLIP box after locating the destination, then
	 * do a RPC locate to find another server listening to the same port.
	 */
	if(!(st->st_flag & F_RETRANS) && (st->st_retrial <= FIRSTLOCATE ||
					  flflags & FLIP_INVAL)) {
	    DPRINTF(0, ("deliverreq: still not reachable: remove port %d\n",
		    st->st_retrial));

	    if(st->st_flag & F_UNTRUSTED) {
		/* No safe path to server; encryption in needed. */
		MON_EVENT("deliverreq: no safe path to server");
		MAKE_IDLE(st);
		st->st_flag &= ~(F_SIGNAL | F_UNTRUSTED | F_UNREACHABLE);
		st->st_rcnt = RPC_UNSAFE;
		return;
	    }
	    st->st_flag &= ~(F_SIGNAL | F_UNREACHABLE | F_NACKED | F_UNTRUSTED);
 	    port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
	    st->st_cache = 0;

	    if(!findserver(st, &st->st_sport)) { 
		st->st_rcnt = (st->st_flag & F_SIGNAL) ? RPC_ABORTED :
		    RPC_NOTFOUND; 
		st->st_flag &= ~(F_SIGNAL | F_RETRANS);
		return;
	    }
	    assert(st->st_cache);
	    st->st_srtime = st->st_cache->p_roundtrip;
	    st->st_saddr = st->st_cache->p_addr;
	    st->st_state = PUTREQ;
	    /* st->st_retrial = MAX_RPC_RETRIAL; */
	    flflags = 0;
	} else {
	    /* Tell the FLIP-box to forget about its routing info. */
	    if(st->st_flag & F_UNREACHABLE) flflags |= FLIP_INVAL;
	    st->st_flag &= ~(F_NACKED | F_UNREACHABLE | F_UNTRUSTED);
	}
    } while(st->st_state == PUTREQ);
    assert(0);
}

#endif /* UNIX_STREAMS */


/* The request has arrived at the server. Wait for the reply.
 * Once in while the server is polled to see if it is still alive.
 */
static void waitrep(st)
    state_p st;
{
    int f = 0;
    
    do {
	rpc_settimer((long) st);
	if(st->st_state == GETREP) {
	    (void) await_reason((event) &st->st_state, 0L, "waitrep");
	} 
	if(st->st_state != GETREP) {
	    assert(NOCLIENT(st) || (st->st_flag & F_FORWARD));
	    return;
	}
	if(st->st_retrial-- <= 0) {
	    MON_EVENT("waitrep: server crashed");
	    MAKE_IDLE(st);
	    st->st_flag &= ~(F_SIGNAL | F_REASSEM);
	    st->st_reassemble = -1;
	    port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0); 
	    st->st_rcnt = RPC_FAILURE;
	    return;
	}
	if(st->st_retrial <= FIRSTLOCATE) f |= FLIP_INVAL;
	sendenquire(st, f);
	f = 0;
	assert(st->st_state == GETREP);
    } while(1);
    /*NOTREACHED*/
}


/* A thread wants to a transaction with the server that listens to the
 * port stored in hdr1.
 * To improve performance, the thread prepares the acknowledgement for the
 * reply when it is waiting for the reply.
 */
#ifdef SECURITY
long secure_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
#else
long rpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
#endif
    header *hdr1, *hdr2;
    bufptr buf1, buf2;
    f_size_t cnt1, cnt2;
{
    register state_p st;
    thread_t *t = (thread_t *) curthread;
#if RPC_PROFILE
    port reqport;
    command reqcommand;
    unsigned long reqtime, repltime;
    bufsize reqsiz, replsiz;
    objnum reqobj;
#endif /* RPC_PROFILE */
#ifdef UNIX_STREAMS
    struct rpc_device *rpcfd;
#endif /* UNIX_STREAMS */

    END_MEASURE(rpc_client);
    BEGIN_MEASURE(rpc_trans);
    BEGIN_MEASURE(rpc_snd_trans);
    if(!hdr1 || !hdr2) {
        return((bufsize) RPC_BADADDRESS);
    }
    if(NULLPORT(&hdr1->h_port)) {
        return((bufsize) RPC_BADPORT);
    }
#if RPC_PROFILE
    reqtime= getmilli();
    reqport= hdr1->h_port;
    reqobj= prv_number(&hdr1->h_priv);
    reqcommand= hdr1->h_command;
    reqsiz= cnt1;
#endif /* RPC_PROFILE */
    st = RPC_THREADSTATE(t);
    assert(st >= statetab && st < statetab + totthread);
    assert(st->st_proc);
#ifdef SECURITY
    st->st_security = hdr1->h_signature._portbytes[0] ? FLIP_SECURITY : 0;
#endif
    if (cnt1 > 0 && buf1 == 0) {
	return(RPC_BADADDRESS);
    }
    if (cnt2 > 0 && buf2 == 0) {
	if (cnt1 > 0) {
	    uunmap(t, (vir_bytes) buf1, (vir_bytes) cnt1);
	}
	return(RPC_BADADDRESS);
    }
    assert(st->st_rhdr == 0);
    st->st_sdata = buf1;
    st->st_rdata = buf2;
    st->st_scnt = cnt1;
    st->st_rcnt = cnt2;
    st->st_shdr = hdr1;
    st->st_rhdr = hdr2;
    st->st_sport = hdr1->h_port;
    INCSIZEBIN(rpc_prqbin, st->st_scnt);
#ifdef UNIX_STREAMS
    st->st_mytid = mytsn++;
    st->st_retrial = MAX_RPC_RETRIAL;
    assert(!(st->st_flag & F_FORWARD));
    if(!st->st_cache || !(PORTCMP(&st->st_cache->p_pubport, &st->st_sport))) { 
	rpcfd = &rpc_dev[st - statetab];
	rpcfd->lochcnt = 1;
	rpcfd->loctime = 0;
	rpcfd->maxretrial = LOCRETRIAL;
	if (findserver(st, &st->st_sport, rpcfd->lochcnt))
	    deliverreq(st, 0, 0);
    } else {
	deliverreq(st, 0, 0);
    }
#else /* UNIX_STREAMS */
    do {
	st->st_mytid = mytsn++;
	st->st_retrial = MAX_RPC_RETRIAL;
	st->st_flag &= ~F_FORWARD;
	deliverreq(st);
	if (!(st->st_flag & F_FORWARD)) {
	    if(NOCLIENT(st)) {
		break;
	    } else if(st->st_state == GETREP) {
		waitrep(st);
		if(NOCLIENT(st)) {
		    break;
		}
	    } 
	}
	assert((st->st_flag & F_FORWARD) && st->st_state == IDLE);
    } while(st->st_flag & F_FORWARD);
    assert(NOCLIENT(st));
    st->st_flag &= ~F_FORWARD;
    if (cnt1 > 0) {
	uunmap(t, (vir_bytes) buf1, (vir_bytes) cnt1);
    }
    if (cnt2 > 0) {
	uunmap(t, (vir_bytes) buf2, (vir_bytes) cnt2);
    }
    assert(st->st_rhdr == hdr2);
    st->st_rhdr = 0;
#ifdef STATISTICS
    if (st->st_rcnt < 64*1024)
	INCSIZEBIN(rpc_grpbin, st->st_rcnt);
#endif
#if RPC_PROFILE
    repltime= getmilli();
    replsiz= st->st_rcnt;
    profile_rpc(getpid(t), THREADSLOT(t), &reqport, reqobj, reqcommand,
		reqsiz, replsiz, reqtime, repltime);
#endif /* RPC_PROFILE */
    END_MEASURE(rpc_rcv_trans);
    END_MEASURE(rpc_trans);
    BEGIN_MEASURE(rpc_client);
    return(st->st_rcnt);
#endif /* UNIX_STREAMS */
}

static int localrep(server_st)
    register state_p server_st;
{
    register state_p st;
    f_size_t l;

    st = RPC_STATE(server_st->st_local_client, server_st->st_cident);
    if(!(st && WAITREPLY(st) && server_st->st_ctid == st->st_mytid &&
		ADR_EQUAL(&st->st_proc->ps_myaddr, &server_st->st_caddr))) {
	return(0);
    }
    assert(st >= statetab && st < statetab + totthread);
    *st->st_rhdr = *server_st->st_shdr;
#ifdef SECURITY
    st->st_rhdr->h_signature._portbytes[0] = 1;
#endif
    l = MIN(st->st_rcnt, server_st->st_scnt);
    if(l > 0) {	
	assert(st->st_rdata);
	(void) memmove((_VOIDSTAR) st->st_rdata,
				(_VOIDSTAR) server_st->st_sdata, (size_t) l);
    }
    MAKE_IDLE(server_st);
    MAKE_IDLE(st);
    st->st_flag &= ~(F_SIGNAL | F_REASSEM | F_RETRANS);
    st->st_reassemble = -1;
    st->st_rcnt = l;
    wakeup((event) &st->st_state);
    return(1);
}


/* Send the reply. */
static void sendrep(st)
    register state_p st;
{
    register pkt_p msg;
    int r;

#ifndef UNIX
    if(st->st_local_client && localrep(st)) {
	return;
    }
#endif
    
    PKT_GET(msg, &rpc_pool);
    if(msg == 0) {
	DPRINTF(0, ("WARNING: sendrep: out of packets"));
    } else {
	proto_init(msg);
	sethdr(msg, st->st_shdr);
	setamp(msg, st->st_cident, st->st_index, RPC_REPLY, 0, st->st_ctid,
	       (adr_p) 0, (port *) 0); 
	if(st->st_scnt > 0) {
#ifdef UNIX
#ifdef UNIX_STREAMS
	    proto_set_virtual(msg, 0, (long) st->st_thread, (long) 
			      st->st_sdata, st->st_scnt); 
#else /* UNIX_STREAMS */
	    proto_set_virtual(msg, FL_DS_USER, (long) st->st_thread, (long)
			      st->st_sdata, st->st_scnt); 
#endif /* UNIX_STREAMS */
#else /* UNIX */
	    proto_set_virtual(msg, 0, 0, (long) st->st_sdata, st->st_scnt);
#endif /* UNIX */
	}
	STINC(rpc_sreply);
	pkt_set_release(msg, rpc_settimer, (long) st);
	END_MEASURE(rpc_snd_putrep);
#ifdef UNIX
	r = flip_unicast(st->st_proc->ps_ifno, msg, FLIP_SYNC | st->st_security,
			 &st->st_caddr, st->st_proc->ps_ep, sizeof(header) +
			 sizeof(amphdr_t) + st->st_scnt, st->st_ltime); 
#else
	r = flip_unicast(st->st_proc->ps_ifno, msg, st->st_security,
			 &st->st_caddr, st->st_proc->ps_ep, sizeof(header) +
			 sizeof(amphdr_t) + st->st_scnt, st->st_ltime); 
#endif
	if(r < 0) {
	    DPRINTF(0, ("sendrep: flip_unicast -> %d\n", r));
	    pkt_discard(msg);
	}
    }
}


/* The thread is finished with processing a request. Now it wants to send back
 * the reply.
 * To improve performance the thread prepares the next reply while it is
 * waiting for the acknowledgement for this reply.
 */
#ifdef SECURITY
long secure_putrep(hdr, buf, cnt)
#else
long rpc_putrep(hdr, buf, cnt)
#endif
    header *hdr;
    bufptr buf;
    f_size_t cnt;
{
    register state_p st;
    thread_t *t = (thread_t *) curthread;

    END_MEASURE(rpc_doop);
    BEGIN_MEASURE(rpc_snd_putrep);
    if(!hdr) {
	return RPC_BADADDRESS;
    }
    st = RPC_THREADSTATE(t);
    assert(st >= statetab && st < statetab + totthread);
    assert(st->st_proc);
#ifdef SECURITY
    st->st_security = hdr->h_signature._portbytes[0] ? FLIP_SECURITY : 0;
#endif
    st->st_flag &= ~F_SIGTRANS;
    if(!DOOPERATION(st)) {
	DPRINTF(0, ("rpc_putrep: %d strange state %d(0x%x)\n", st->st_index,
		st->st_state, st->st_flag));
	progerror();
	return(RPC_FAILURE);
    }
    if (cnt > 0 && buf == 0) {
	progerror();
	return(RPC_BADADDRESS);
    }
    st->st_sdata = buf;
    st->st_scnt = cnt;
    st->st_shdr = hdr;
    st->st_retrial = MAX_RPC_RETRIAL;
    st->st_flag &= ~F_DOOP;
    st->st_state = PUTREP;
    INCSIZEBIN(rpc_prpbin, st->st_scnt);
#ifdef UNIX_STREAMS
    sendrep(st);
    return STD_OK;
#else /* UNIX_STREAMS */
    do {
	sendrep(st);
	do {
	    if(st->st_state == PUTREP) {
		(void) await_reason((event) &st->st_state, 0L, "putrep");
	    }
	    if(st->st_state != PUTREP) {
		assert(!RPC_ACTIVE(st));
		if (cnt > 0) {
		    uunmap(t, (vir_bytes) buf, (vir_bytes) cnt);
		}
		END_MEASURE(rpc_rcv_putrep);
		return(STD_OK);
	    } 
	    if(st->st_retrial-- <= 0) {
		MON_EVENT("rpc_putrep: putrep failed");
		MAKE_IDLE(st);
		st->st_flag = 0;
		assert(st->st_reassemble < 0);
		if (cnt > 0) {
		    uunmap(t, (vir_bytes) buf, (vir_bytes) cnt);
		}
		return(RPC_FAILURE);
	    }
	    if(!(st->st_flag & F_NACKED)) {
		DPRINTF(0, ("putrep: send received %d\n", st->st_ctid));
		sendreceived(st, &st->st_caddr, st->st_cident, st->st_index,
			     st->st_ctid, FL_REPLY);
	    }
	} while(!(st->st_flag & F_NACKED));
	st->st_flag &= ~F_NACKED;
    } while(st->st_state == PUTREP);
    assert(0);
    /*NOTREACHED*/
    return(RPC_FAILURE);
#endif /* UNIX_STREAMS */
}


#ifdef UNIX_STREAMS

int wakeup_putrep(state_p st)
{
    struct rpc_device *rpcfd;
    struct rpc_args *rpc_args;
    mblk_t *mp;

    if(st->st_retrial-- <= 0) {
	MON_EVENT("rpc_putrep: putrep failed");
	MAKE_IDLE(st);
	st->st_flag = 0;
	assert(st->st_reassemble < 0);
	/*
	if (cnt > 0) {
	    uunmap(t, (vir_bytes) buf, (vir_bytes) cnt);
	}
	*/
	rpcfd = &rpc_dev[st - statetab];
	st->st_rhdr = 0;
	mp = rpcfd->mp;
	freemsg(rpcfd->mp_data);
	rpcfd->mp_data = 0;
	rpc_args = (struct rpc_args *) DB_BASE(mp);
	rpc_args->rpc_status = RPC_FAILURE;
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + sizeof(struct rpc_args);
	qreply(rpcfd->queue, mp);
    }
    if(!(st->st_flag & F_NACKED)) {
	DPRINTF(0, ("putrep: send received %d\n", st->st_ctid));
	sendreceived(st, &st->st_caddr, st->st_cident, st->st_index,
		     st->st_ctid, FL_REPLY);
    } else {
	st->st_flag &= ~F_NACKED;
	sendrep(st);
    }
}

#endif /* UNIX_STREAMS */


void rpc_cleanup()
{
    progerror();
}


#if defined(UNIX) && defined(SECURITY)
long rpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
    header *hdr1, *hdr2;
    bufptr buf1, buf2;
    f_size_t cnt1, cnt2;
{
	hdr1->h_signature._portbytes[0]= 1;
	return secure_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2);
}

long rpc_getreq(hdr, buf, cnt)
    header *hdr;
    bufptr buf;
    f_size_t cnt;
{
	return secure_getreq(hdr, buf, cnt);
}

long rpc_putrep(hdr, buf, cnt)
    header *hdr;
    bufptr buf;
    f_size_t cnt;
{
	hdr->h_signature._portbytes[0]= 1;
	return secure_putrep(hdr, buf, cnt);
}
#endif /* defined(UNIX) && defined(SECURITY) */


#ifdef FLIPGRP

static int localforward(server_st, newaddr)
    register state_p server_st;
    adr_p newaddr;
{
    register state_p st = RPC_STATE(server_st->st_local_client,
					   server_st->st_cident);

    if(!(st && WAITREPLY(st) && server_st->st_ctid ==
	   st->st_mytid && ADR_EQUAL(&st->st_proc->ps_myaddr,
					    &server_st->st_caddr))) {
	return(0);
    }

    /* Remove current server from the port cache, and add the new one */
    port_remove(&st->st_sport, &st->st_saddr, st->st_sident, 0);
    port_install(st->st_sident, newaddr, &st->st_sport, P_SOMEWHERE,
		 st->st_srtime, (char *) 0);

    MAKE_IDLE(server_st);
    st->st_saddr = *newaddr;
    MAKE_IDLE(st);
    st->st_reassemble = -1;
    st->st_flag &= ~(F_SIGNAL | F_REASSEM | F_RETRANS);
    st->st_flag |= F_FORWARD;
#ifdef UNIX_STREAMS
    if (st->st_state == PUTREQ) {
	wakeup_putreq(st);
    } else {    /* st->st_state == GETREP */
	wakeup_getrep(st);
    }
#else
    wakeup((event) &st->st_state);
#endif
    return(1);
}

/* Send forward */
static void sendforward(st, memaddr)
    register state_p st;
    adr_p memaddr;
{
    register pkt_p msg;
    int r;

#ifndef UNIX
    if(st->st_local_client && localforward(st, memaddr)) {
	return;
    }
#endif

    PKT_GET(msg, &rpc_pool);
    if(msg == 0) {
	DPRINTF(0, ("WARNING: sendforward: out of packets"));
    } else {
	proto_init(msg);
	setamp(msg, st->st_cident, st->st_index, RPC_FORWARD, 0, st->st_ctid,
	       (adr_p) 0, (port *) 0);
	proto_dir_append(msg, (char *) memaddr, sizeof(adr_t));
	STINC(rpc_sforward);
	pkt_set_release(msg, rpc_settimer, (long) st);
#ifdef UNIX
	r = flip_unicast(st->st_proc->ps_ifno, msg, st->st_security | FLIP_SYNC,
			 &st->st_caddr, st->st_proc->ps_ep, (f_size_t)
			 sizeof(amphdr_t) + sizeof(adr_t), st->st_ltime); 
#else
	r = flip_unicast(st->st_proc->ps_ifno, msg, st->st_security,
			 &st->st_caddr, st->st_proc->ps_ep, (f_size_t)
			 sizeof(amphdr_t) + sizeof(adr_t), st->st_ltime);  
#endif
	if(r < 0) {
	    DPRINTF(0, ("sendforward: flip_unicast -> %d\n", r));
	    pkt_discard(msg);
	}
    }
}


/* Forward a request to another group member. */
errstat grp_forward(gd, p, memid)
    g_id_t gd;
    port *p;
    g_indx_t memid;
{
    register state_p st;
    thread_t *t = curthread;
    adr_t memaddr;
    
    st = RPC_THREADSTATE(t);
    assert(st >= statetab && st < statetab + totthread);
    assert(st->st_proc);
#ifdef SECURITY
    st->st_security = hdr->h_signature._portbytes[0] ? FLIP_SECURITY : 0;
#endif
    st->st_flag &= ~F_SIGTRANS;
    if(!DOOPERATION(st)) {
	DPRINTF(0, ("grp_forward: %d strange state %d(0x%x)\n", st->st_index,
		    st->st_state, st->st_flag));
	progerror();
	return(RPC_FAILURE);
    }
    if(grp_memaddress(p, gd, memid, &memaddr) < 0) 
    {
	return(RPC_FAILURE);
    }
    st->st_retrial = MAX_RPC_RETRIAL;
    st->st_flag &= ~F_DOOP;
    st->st_scnt = 0;
    st->st_state = PUTREP;
    do {
	sendforward(st, &memaddr);
	do {
	    if(st->st_state == PUTREP) {
		(void) await_reason((event) &st->st_state, 0L, "grp_forward");
	    }
	    if(st->st_state != PUTREP) {
		assert(!RPC_ACTIVE(st));
		return(STD_OK);
	    }
	    if(st->st_retrial-- <= 0) {
		DPRINTF(0, ("%d: forward failed %D\n", st->st_index,
			    st->st_ctid));
		MAKE_IDLE(st);
		st->st_flag = 0;
		assert(st->st_reassemble < 0);
		return(RPC_ABORTED);
	    }
	    if(!(st->st_flag & F_NACKED)) {
		sendreceived(st, &st->st_caddr, st->st_cident, st->st_index,
			     st->st_ctid, FL_REPLY);
	    }
	} while(!(st->st_flag & F_NACKED));
    } while(st->st_state == PUTREP);
    assert(0);
    /*NOTREACHED*/
    return(RPC_FAILURE);
}
#endif

#ifndef SMALL_KERNEL
/* Write information about the rpc module into a buffer. */

static int dumpflags(begin, end, f)
    char *begin;
    char *end;
    uint32 f;
{
    char *p;
    
    p = begin;
    p = bprintf(p, end, " ( ");
    if(f & F_DOOP) p = bprintf(p, end, "doop ");
    if(f & F_NACKED) p = bprintf(p, end, "nacked ");
    if(f & F_REASSEM) p = bprintf(p, end, "receiving ");
    if(f & F_FORWARD) p = bprintf(p, end, "forward ");
    if(f & F_SIGNAL) p = bprintf(p, end, "signaled ");
    if(f & F_RETRANS) p = bprintf(p, end, "retrans ");
    if(f & F_UNREACHABLE) p = bprintf(p, end, "unreachable ");
    if(f & F_SIGTRANS) p = bprintf(p, end, "sigtrans ");
    if(f & F_UNTRUSTED) p = bprintf(p, end, "untrusted ");
    p = bprintf(p, end, ")");
    return(p - begin);
}


static int dumpstate(begin, end, st)
    char *begin, *end;
    state_p st;
{
    char *p = begin;

    switch(st->st_state) {
    case IDLE:
	p = bprintf(p, end, "idle");
	break;
    case GETREQ:
	p = bprintf(p, end, "getreq");
	break;
    case PUTREP:
	p = bprintf(p, end, "putrep");
	break;
    case LOCATE:
	p = bprintf(p, end, "locate");
	break;
    case PUTREQ:
	p = bprintf(p, end, "putreq");
	break;
    case GETREP:
	p = bprintf(p, end, "getrep");
	break;
    default:
	panic("dumpstate: illegal state");
	break;
    }
    p += dumpflags(p, end, st->st_flag);
    return(p - begin);
}


int rpc_dump(begin, end)
    char *begin;
    char *end;
{
    char *p;
    procstate_p proc;
    state_p *t, st;
    
    p = bprintf(begin, end, "======== rpc dump ===  ");
    p += badr_print(p, end, &mykid);
    p = bprintf(p, end, "  ===\n");
    for(proc = proctab; proc < proctab + nproc; proc++) {
	if(!proc->ps_init) continue;
	p += badr_print(p, end, &proc->ps_myaddr);
	p = bprintf(p, end, " (pid %d) : %d thread%s\n",
	    proc->ps_pid, proc->ps_nthread, proc->ps_nthread == 1 ? "" : "s");
	for(t = proc->ps_thread; t < proc->ps_thread + nthread_proc; t++) {
	    st = *t;
	    if(st == 0) continue;
	    p = bprintf(p, end, "%d(%d): tid %D ", THREADSLOT(st->st_thread),
			st->st_index, st->st_mytid);
	    p += dumpstate(p, end, st);
	    p = bprintf(p, end, 
			" timer: %d retrial %d loctime %d scnt %d rcnt %d\n", 
			st->st_timeout, st->st_retrial, st->st_ltime,
			st->st_scnt, st->st_rcnt);
	    if(SERVER(st)) {
		p = bprintf(p, end, "    ctid %d cident %d cadr: ",
			    st->st_ctid, st->st_cident);
		p += badr_print(p, end, &st->st_caddr);
		p = bprintf(p, end, " rtime %d\n", st->st_crtime);
		p = bprintf(p, end, "    port: pub %P\n", &st->st_pubport); 
	    }
	    if(CLIENT(st)) {
		p = bprintf(p, end, "    cache: %P sid %d saddr: ",
			    &st->st_sport, st->st_sident);
		p += badr_print(p, end, &st->st_saddr);
		p = bprintf(p, end, " rtime %d\n", st->st_srtime);
	    }
	    if(st->st_state == LOCATE) {
		p = bprintf(p, end, "    locating %P deltatime %d\n", 
			    &st->st_sport, st->st_deltatime);
	    }
	    if(st->st_flag & F_REASSEM)  {
		p = bprintf(p, end, "    roff %d rmid %d timer: %d\n",
			    st->st_roffset, st->st_rmessid, st->st_reassemble);
	    }
	}
    }
    p = bprintf(p, end, "==========================================\n");
    return(p - begin);
}


#ifdef STATISTICS
rpc_statistics(begin, end)
    char *begin;
    char *end;
{
    char *p;
    int i;
    
    p = bprintf(begin, end, "======== FLIP AMRPC statistics =======\n");
    p = bprintf(p, end, "send statistics:\n");
    p = bprintf(p, end, "slocate  %7ld ", amstat.rpc_slocate);
    p = bprintf(p, end, "shereis  %7ld ", amstat.rpc_shereis);
    p = bprintf(p, end, "srequest %7ld ", amstat.rpc_srequest);
    p = bprintf(p, end, "sreply   %7ld\n", amstat.rpc_sreply);
    p = bprintf(p, end, "sack     %7ld ", amstat.rpc_sack);
    p = bprintf(p, end, "snak     %7ld ", amstat.rpc_snak);
    p = bprintf(p, end, "salive   %7ld ", amstat.rpc_salive);
    p = bprintf(p, end, "senquire %7ld\n", amstat.rpc_senquire);
    p = bprintf(p, end, "sfail    %7ld ", amstat.rpc_sfail);
    p = bprintf(p, end, "sforward %7ld ", amstat.rpc_sforward);
    p = bprintf(p, end, "sreceive %7ld ", amstat.rpc_sreceived);
    p = bprintf(p, end, "local    %7ld\n", amstat.rpc_localreq); 
    p = bprintf(p, end, "receive statistics:\n");
    p = bprintf(p, end, "rlocate  %7ld ", amstat.rpc_rlocate);
    p = bprintf(p, end, "rhereis  %7ld ", amstat.rpc_rhereis);
    p = bprintf(p, end, "rrequest %7ld ", amstat.rpc_rrequest);
    p = bprintf(p, end, "rreply   %7ld\n", amstat.rpc_rreply);
    p = bprintf(p, end, "rack     %7ld ", amstat.rpc_rack);
    p = bprintf(p, end, "rnak     %7ld ", amstat.rpc_rnak);
    p = bprintf(p, end, "ralive   %7ld ", amstat.rpc_ralive);
    p = bprintf(p, end, "renquire %7ld\n", amstat.rpc_renquire);
    p = bprintf(p, end, "rfail    %7ld ", amstat.rpc_rfail);
    p = bprintf(p, end, "rforward %7ld ", amstat.rpc_rforward);
    p = bprintf(p, end, "rreceive %7ld\n", amstat.rpc_rreceived);
    p = bprintf(p, end, "timeout %7ld\n", amstat.rpc_timeout);
    p = bprintf(p, end, "\nbin#\t    grq     prp     prq     grp\n");
    for (i = 0; i < SIZEBINSIZE; i++)
        p = bprintf(p, end, "%3d\t%7ld %7ld %7ld %7ld\n", i,
                        amstat.rpc_grqbin[i],
                        amstat.rpc_prpbin[i],
                        amstat.rpc_prqbin[i],
                        amstat.rpc_grpbin[i]);

    return p - begin;
}
#endif /* STATISTICS */

#endif /* SMALL_KERNEL */
