/*	@(#)rawflip.c	1.4	96/02/27 14:03:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "stderr.h"
#include "assert.h"
INIT_ASSERT

#include "map.h"
#include "machdep.h"
#include "global.h"
#include "exception.h"
#include "kthread.h"
#include "debug.h"
#include "sys/flip/measure.h"
#include "protocols/rawflip.h"
#include "sys/proto.h"

#define RF_NPKTS	30
#define RF_PKTSIZE	300		/* No direct data so this is enough */
#define RF_CPYSIZE	200		/* Cannot be larger than ff_pktsize */

static pool_t raw_pool;
static pkt_p  raw_pkt;
static char * raw_pktdata;

static
struct rawif {
	int	rif_proc;		/* Process this interface is on */
	int	rif_ifno;		/* Interface number */
	int	rif_active;		/* Process wants to quit */
	int	rif_upc_state;		/* state of upcalls */
	pkt_p	rif_iqueue;		/* Input queue */
#ifdef STATISTICS
	struct {
		int	rifs_runicast;
		int	rifs_rmulticast;
		int	rifs_rbroadcast;
		int	rifs_rreceive;
		int	rifs_rnotdeliver;

		int	rifs_sunicast;
		int	rifs_smulticast;
		int	rifs_sbroadcast;
		int	rifs_sreceive;
		int	rifs_snotdeliver;

		int	rifs_upcall;
		int	rifs_flushed;
	}	rif_stats;
#endif
} *rawif, *lastrawif;

#ifdef STATISTICS
#define STINC(rif, stat)	(rif)->rif_stats.stat++
#else
#define STINC(rif, stat)	{}
#endif

#define USTATE_IDLE1		1	/* Idle for less than 1 sweep */
#define USTATE_IDLE2		2	/* Idle between 1 and 2 sweeps */
#define USTATE_IDLE_LONG	3	/* Idle too long */
#define USTATE_WAITING		4	/* Waiting for message */

static
struct rawif *
rawflip_search(ifno)
{
	int pid;
	struct rawif *rip;

	pid = getpid(curthread);
	for (rip=rawif; rip <= lastrawif; rip++) {
		if (rip->rif_proc == pid && rip->rif_ifno == ifno)
			return rip;
	}
	return 0;
}

static void
rawflip_flush_queue(ppp)
pkt_p *ppp;
{
	pkt_p pkt;

	while ((pkt = *ppp) != 0) {
		*ppp = pkt->p_admin.pa_next;
		pkt->p_admin.pa_next = 0;
		pkt_discard(pkt);
	}
}

static void
raw_receive(ident, pkt, dst, src, messid, offset, length, total, flag) 
pkt_p pkt;
adr_p dst, src;
f_msgcnt_t messid;
f_size_t offset, length, total;
int flag;
{
	struct rawif *rip;
	pkt_p *ppp;
	struct rawflip_upcall *rfu;

	BEGIN_MEASURE(raw_receive);
	rip = (struct rawif *) ident;
	STINC(rip, rifs_rreceive);
	DPRINTF(1, ("packet arrived for process %d, ", rip->rif_proc));
	if (!rip->rif_active || rip->rif_upc_state==USTATE_IDLE_LONG) {
		DPRINTF(1, ("discarding\n"));
		pkt_discard(pkt);
		return;
	}
	PROTO_ADD_HEADER(rfu, pkt, struct rawflip_upcall);
	rfu->rfu_src = *src;
	rfu->rfu_dst = *dst;
	rfu->rfu_messid = messid;
	rfu->rfu_offset = offset;
	rfu->rfu_len = length;
	rfu->rfu_tlen = total;
	rfu->rfu_flags = flag;
	rfu->rfu_result = UPCALL_RESULT_RECEIVED;
	proto_fix_header(pkt);
	DPRINTF(1, ("queueing pkt %x\n", pkt));
	for(ppp = &rip->rif_iqueue; *ppp; ppp = &(*ppp)->p_admin.pa_next)
		;
	*ppp = pkt;
	STINC(rip, rifs_sreceive);
	wakeup((event) &rip->rif_iqueue);
}

static void
raw_notdeliver(ident, pkt, dst, messid, offset, length, total, flag) 
pkt_p pkt;
adr_p dst;
f_msgcnt_t messid;
f_size_t offset, length, total;
int flag;
{
	struct rawif *rip;
	pkt_p *ppp;
	struct rawflip_upcall *rfu;

	rip = (struct rawif *) ident;
	STINC(rip, rifs_rnotdeliver);
	DPRINTF(1, ("packet not delivered for process %d, ", rip->rif_proc));
	if (!rip->rif_active || rip->rif_upc_state==USTATE_IDLE_LONG) {
		DPRINTF(1, ("discarding\n"));
		pkt_discard(pkt);
		return;
	}
	PROTO_ADD_HEADER(rfu, pkt, struct rawflip_upcall);
	rfu->rfu_dst = *dst;
	rfu->rfu_messid = messid;
	rfu->rfu_offset = offset;
	rfu->rfu_len = length;
	rfu->rfu_tlen = total;
	rfu->rfu_flags = flag;
	rfu->rfu_result = UPCALL_RESULT_NDLIVERED;
	proto_fix_header(pkt);
	DPRINTF(1, ("queueing pkt %x\n", pkt));
	for(ppp = &rip->rif_iqueue; *ppp; ppp = &(*ppp)->p_admin.pa_next)
		;
	*ppp = pkt;
	STINC(rip, rifs_snotdeliver);
	wakeup((event) &rip->rif_iqueue);
}

static void
rawflip_cleanup(rip)
struct rawif *rip;
{
	rip->rif_proc = -1;
	rip->rif_ifno = -1;
	rip->rif_active = 0;
	rip->rif_upc_state = USTATE_IDLE1;
	STINC(rip, rifs_flushed);
	rawflip_flush_queue(&rip->rif_iqueue);
}

static void
rawflip_do_end(rip)
struct rawif *rip;
{
	if (rip->rif_ifno >= 0) {
		if (flip_end(rip->rif_ifno) != FLIP_OK) {
			printf("flip_end(%d) failed\n", rip->rif_ifno);
		}
		rip->rif_ifno = -1;
	}
}

static int
rawflip_init()
{
	int pid;
	struct rawif *rip,*candidate;

	candidate = 0;
	pid = getpid(curthread);
	DPRINTF(0, ("process %d does flip_init()\n", pid));
	for (rip=rawif; rip <= lastrawif; rip++) {
		if (rip->rif_proc == pid) {
			rawflip_do_end(rip);
			rawflip_cleanup(rip);
			candidate = rip;
			break;
		}
		if (rip->rif_proc == -1 && candidate == 0) 
			candidate = rip;
	}
	if (candidate == 0)
		return FLIP_FAIL;
	candidate->rif_ifno = flip_init((long) candidate, raw_receive,
					raw_notdeliver);
	if (candidate->rif_ifno < 0)
		return FLIP_FAIL;
	candidate->rif_proc = pid;
	candidate->rif_iqueue = 0;
	candidate->rif_active = 1;
	candidate->rif_upc_state = USTATE_IDLE1;
#ifdef STATISTICS
	(void) memset((_VOIDSTAR) &candidate->rif_stats, 0,
		      sizeof(candidate->rif_stats));
#endif
	DPRINTF(0, ("return ifno %d\n", candidate->rif_ifno));
	return candidate->rif_ifno;
}

static int
rawflip_end(ifno)
{
	struct rawif *rip;

	if (ifno < 0)
		return FLIP_FAIL;
	rip = rawflip_search(ifno);
	if (rip == 0)
		return FLIP_FAIL;
	rawflip_cleanup(rip);
	wakeup((event) &rip->rif_iqueue);
	return flip_end(ifno);
}

#ifndef NO_MPX_REGISTER
static void
rawflip_exit(t)
struct thread *t;
{
	/* If the thread now leaving is the last, make sure all the rawflip
	 * resources of the process are freed.
	 */
	int pid;
	struct rawif *rip;

	if (mpx_nthread(t) == 1) {
		pid = getpid(t);
		for (rip = rawif; rip <= lastrawif; rip++) {
			if (rip->rif_proc == pid) {
				/* Don't call rawflip_end since it implicitly
				 * uses curthread, which we don't want in this
				 * case.
				 */
				rawflip_do_end(rip);
				rawflip_cleanup(rip);
			}
		}
	}
}
#endif

static int
rawflip_register(ifno, addr)
adr_p addr;
{
	struct rawif *rip;

	rip = rawflip_search(ifno);
	if (rip == 0)
		return FLIP_FAIL;
	return flip_register(ifno, addr);
}

static int
rawflip_unregister(ifno, ep)
int ifno, ep;
{
	struct rawif *rip;

	rip = rawflip_search(ifno);
	if (rip == 0)
		return FLIP_FAIL;
	return flip_unregister(ifno, ep);
}

/*ARGSUSED*/
static pkt_p
rawflip_pkt_init(rip, uaddr, length)
struct rawif *rip;
vir_bytes uaddr;
f_size_t length;
{
	vir_bytes kaddr;
	pkt_p msg;
	f_size_t l;

	kaddr = PTOV(umap(curthread, uaddr, length, 0));
	if (kaddr == 0) {
		printf("rawflip_pkt_init: umap(0x%lx, %ld) failed\n",
			uaddr, length);
		progerror();
		return 0;
	}
	PKT_GET(msg, &raw_pool);
	if (msg == 0)
		return msg;
	proto_init(msg);

	/*
	 * Go copy first part of message
	 * Calculate room left first
	 */
	l = RF_CPYSIZE - msg->p_contents.pc_offset;
	if (l > length)
		l = length;
	proto_dir_append(msg, kaddr, l);
	kaddr += l;
	length -= l;

	if (length>0) {
		/*
		 * "Virtually" append the rest
		 */
		proto_set_virtual(msg, 0, 0, kaddr, length);
	}
	return msg;
}

static int
rawflip_broadcast(ifno, uaddr, ep, length, hopcnt)
int ifno;
vir_bytes uaddr;
int ep;
f_size_t length;
f_hopcnt_t hopcnt;
{
	struct rawif *rip;
	pkt_p msg;
	int result;

	rip = rawflip_search(ifno);
	if (rip == 0)
		return FLIP_FAIL;
	STINC(rip, rifs_rbroadcast);
	msg = rawflip_pkt_init(rip, uaddr, length);
	if (msg == 0)
		return FLIP_NOPACKET;
	END_MEASURE(raw_send);
	STINC(rip, rifs_sbroadcast);
	result = flip_broadcast(ifno, msg, ep, length, hopcnt);
	if (result != FLIP_OK)
		pkt_discard(msg);
	return result;
}

static int
rawflip_xxcast(ifno, uaddr, flags, dst, ep, length, n, ltime)
int ifno;
vir_bytes uaddr;
int flags;
adr_p dst;
int ep;
f_size_t length;
int n;
interval ltime;
{
	struct rawif *rip;
	pkt_p msg;
	int result;

	rip = rawflip_search(ifno);
	if (rip == 0)
		return FLIP_FAIL;
	if (n == 0) {
		STINC(rip, rifs_runicast);
	} else {
		STINC(rip, rifs_rmulticast);
	}
	msg = rawflip_pkt_init(rip, uaddr, length);
	if (msg == 0)
		return FLIP_NOPACKET;
	END_MEASURE(raw_send);
	if (n == 0) {
		STINC(rip, rifs_sunicast);
		result = flip_unicast(ifno, msg, flags, dst, ep, length,
				      ltime);
	} else {
		STINC(rip, rifs_smulticast);
		result = flip_multicast(ifno, msg, flags, dst, ep, length,
							(uint16) n, ltime);
	}
	if (result != FLIP_OK)
		pkt_discard(msg);
	return result;
}

static void
rawflip_upcall(arg)
struct rawflip_parms *arg;
{
	struct rawif *rip;
	pkt_p msg;
	struct rawflip_upcall *rfu;
	vir_bytes uaddr, kaddr, length, bytesleft, l;
	
	rip = rawflip_search(arg->rf_if);
	if (rip == 0) {
		DPRINTF(1, ("rawflip_upcall: ifno %d not found\n", arg->rf_if));
		arg->rf_result = UPCALL_RESULT_EXIT;
		return;
	}

	/* Shorten the critical path by mapping the pointer to the user's
	 * packet buffer in advance.
	 */
	uaddr = (vir_bytes) arg->rf_pkt;
	bytesleft = length = arg->rf_len;
	kaddr = PTOV(umap(curthread, uaddr, length, 0));
	if (kaddr == 0) {
		printf("rawflip_upcall: umap(0x%lx, %ld) failed\n",
			uaddr, length);
		progerror();
		return;
	}

	STINC(rip, rifs_upcall);
	while (rip->rif_active && rip->rif_iqueue == 0) {
		rip->rif_upc_state = USTATE_WAITING;
		DPRINTF(1, ("rawflip_upcall waiting on iqueue 0x%lx\n",
			    &rip->rif_iqueue));
		if (await((event) &rip->rif_iqueue, (interval) 0) < 0) {
			DPRINTF(2, ("rawflip_upcall interrupted\n"));
			arg->rf_result = UPCALL_RESULT_ABORTED;
			rip->rif_upc_state = USTATE_IDLE1;
			return;
		}
	}
	if (!rip->rif_active) {
		DPRINTF(1, ("rawflip_upcall: not active\n"));
		arg->rf_result = UPCALL_RESULT_EXIT;
		return;
	}
	/* We put the END_MEASURE of raw_receive here, under the assumption
	 * that the process will typically be waiting for the next packet
	 * to arrive.  The wait time till the next packet arrives is not in
	 * the critical path, of course.
	 */
	END_MEASURE(raw_receive);
	BEGIN_MEASURE(raw_upcall);

	msg = rip->rif_iqueue;

	PROTO_LOOK_HEADER(rfu, msg, struct rawflip_upcall);
	arg->rf_u = *rfu;
	proto_remove_header(msg);

	l = msg->p_contents.pc_dirsize;
	if (l > bytesleft)
		l = bytesleft;
	if (l) {
		/* copy direct data */
		(void) memmove((_VOIDSTAR) kaddr,
				(_VOIDSTAR) pkt_offset(msg), (size_t) l);
		kaddr += l;
		bytesleft -= l;
	}
	msg->p_contents.pc_dirsize -= l;
	msg->p_contents.pc_totsize -= l;

	/*
	 * At this point we copied all the direct data that would fit
	 * Now either we are out of direct data, or the buffer is full
	 * In both cases we can safely go on with the copying of indirect
	 * data: in the first case it is the right thing to do, and in
	 * the second case it will do nothing anyhow.
	 */
	assert(msg->p_contents.pc_dirsize == 0 || bytesleft == 0);

	l = msg->p_contents.pc_totsize;
	if (l > bytesleft)
		l = bytesleft;
	if (l) {
		/* copy user data */
		(void) memmove((_VOIDSTAR) kaddr,
			(_VOIDSTAR) msg->p_contents.pc_virtual, (size_t) l);
		msg->p_contents.pc_totsize -= l;
		msg->p_contents.pc_virtual += l;
	}

	if (msg->p_contents.pc_totsize == 0 ) {
		/*
		 * Done with packet
		 */
		rip->rif_iqueue = msg->p_admin.pa_next;
		msg->p_admin.pa_next = 0;
		pkt_discard(msg);
	} else {
		PROTO_ADD_HEADER(rfu, msg, struct rawflip_upcall);
		*rfu = arg->rf_u;
		rfu->rfu_offset += length;
		rfu->rfu_len -= length;
		proto_fix_header(msg);
		arg->rf_len = length;
	}
	rip->rif_upc_state = USTATE_IDLE1;
}

static
rawflip(arg)
struct rawflip_parms *arg;
{

	switch(arg->rf_opcode) {
	case RF_OPC_UPCALL:
		/* no BEGIN_MEASURE here; see comment in rawflip_upcall() */
		rawflip_upcall(arg);
		END_MEASURE(raw_upcall);
		/* An incoming unicast message typically generates a reply;
		 * we're interested in the delay (including thread switches)
		 * involved.
		 */
		BEGIN_MEASURE(raw_doop);
		break;
	case RF_OPC_INIT:
		arg->rf_if = rawflip_init();
		break;
	case RF_OPC_END:
		arg->rf_result = rawflip_end(arg->rf_if);
		break;
	case RF_OPC_REGISTER:
		arg->rf_ep = rawflip_register(arg->rf_if, &arg->rf_src);
		break;
	case RF_OPC_UNREGISTER:
		arg->rf_result = rawflip_unregister(arg->rf_if, arg->rf_ep);
		break;
	case RF_OPC_BROADCAST:
		BEGIN_MEASURE(raw_send);
		arg->rf_result = rawflip_broadcast(arg->rf_if,
				(vir_bytes) arg->rf_pkt,
				arg->rf_ep, arg->rf_len, arg->rf_hop);
		break;
	case RF_OPC_UNICAST:
		/* Assume this is a reply for a recent request */
		END_MEASURE(raw_doop);
		BEGIN_MEASURE(raw_send);
		arg->rf_result = rawflip_xxcast(arg->rf_if,
			(vir_bytes) arg->rf_pkt,
			arg->rf_flags, &arg->rf_dst, arg->rf_ep, arg->rf_len,
			0, arg->rf_ltime);
		break;
	case RF_OPC_MULTICAST:
		/* A 'sequencer' member responds with multicast messages
		 * to the group, but it could also be an independent
		 * multicast, of course.
		 */
		END_MEASURE(raw_doop);
		BEGIN_MEASURE(raw_send);
		arg->rf_result = rawflip_xxcast(arg->rf_if,
			(vir_bytes) arg->rf_pkt,
			arg->rf_flags, &arg->rf_dst, arg->rf_ep, arg->rf_len,
			arg->rf_n, arg->rf_ltime);
		break;
	default:
		return STD_ARGBAD;
	}
	return STD_OK;
}

/*ARGSUSED*/
static void
raw_sweep(arg)
long arg;
{
	struct rawif *rip;

	for (rip=rawif; rip <= lastrawif; rip++) {
		if (rip->rif_active == 0)
			continue;
		switch(rip->rif_upc_state) {
		case USTATE_IDLE1:
			rip->rif_upc_state = USTATE_IDLE2;
			break;
		case USTATE_IDLE2:
			rip->rif_upc_state = USTATE_IDLE_LONG;
			STINC(rip, rifs_flushed);
			rawflip_flush_queue(&rip->rif_iqueue);
			break;
		}
	}
}

void
rawflip_startup()
{
	extern uint16 nproc;
	struct rawif *rip;

	rawif = (struct rawif *) aalloc((vir_bytes) nproc*sizeof(*rawif), 0);
	lastrawif = rawif + nproc-1;

	for (rip = rawif; rip <= lastrawif; rip++) {
		rip->rif_active = 0;
		rip->rif_ifno = -1;
		rip->rif_proc = -1;
		/* By using -1 instead of 0 as invalid rif_proc it should
		 * be possible us to call rawflip from the kernel, if we
		 * want to.
		 */
	}

	DPRINTF(0, ("rawflip_startup: aalloc %d pkts of size %d\n",
				RF_NPKTS, RF_PKTSIZE));
	raw_pkt = (pkt_p) aalloc((vir_bytes)(sizeof(pkt_t) * RF_NPKTS), 0);
	raw_pktdata = aalloc((vir_bytes)(RF_NPKTS * RF_PKTSIZE), 0);
	pkt_init(&raw_pool, RF_PKTSIZE, raw_pkt, RF_NPKTS, raw_pktdata,
			(void (*)()) 0, 0L);
	sweeper_set(raw_sweep, 0L, 4000L, 0);
	register_rawflip(rawflip);
#ifndef NO_MPX_REGISTER
	/* we only need to register the thread exit function for raw flip */
	mpx_register((void (*)()) 0, (int (*)()) 0,
		     (void (*)()) 0, rawflip_exit);
#endif
}

#ifndef SMALL_KERNEL

static char *state[] = {
	"zero", "IDLE1", "IDLE2", "IDLE", "WAITING", 
};

rawflip_dump(begin, end)
char *begin, *end;
{
	char *p;
	struct rawif *rip;

	p = bprintf(begin, end, "Raw flip interface dump\n");
	p = bprintf(p, end, "Int\tProc\tIfno\tState\tIqueue\n");
	for (rip=rawif; rip <= lastrawif; rip++) {
		if (rip->rif_active == 0)
			continue;
		p = bprintf(p, end, "%3d\t%3d\t%3d\t%s\t%x\n", rip - rawif,
			    rip->rif_proc, rip->rif_ifno,
			    state[rip->rif_upc_state], rip->rif_iqueue);
	}

#ifdef STATISTICS
	p = bprintf(p, end, "\nRaw flip statistics:\n");
	for (rip=rawif; rip <= lastrawif; rip++) {
		if (rip->rif_active == 0)
			continue;

		p = bprintf(p, end, "\nPid %3d:\n", rip->rif_proc);

		p = bprintf(p, end, "runicast %5ld ",
			    rip->rif_stats.rifs_runicast);
		p = bprintf(p, end, "rmulticast %5ld ",
			    rip->rif_stats.rifs_rmulticast);
		p = bprintf(p, end, "rbroadcast %3ld ",
			    rip->rif_stats.rifs_rbroadcast);
		p = bprintf(p, end, "rreceive %5ld ",
			    rip->rif_stats.rifs_rreceive);
		p = bprintf(p, end, "rnotdeliver %5ld\n",
			    rip->rif_stats.rifs_rnotdeliver);

		p = bprintf(p, end, "sunicast %5ld ",
			    rip->rif_stats.rifs_sunicast);
		p = bprintf(p, end, "smulticast %5ld ",
			    rip->rif_stats.rifs_smulticast);
		p = bprintf(p, end, "sbroadcast %3ld ",
			    rip->rif_stats.rifs_sbroadcast);
		p = bprintf(p, end, "sreceive %5ld ",
			    rip->rif_stats.rifs_sreceive);
		p = bprintf(p, end, "snotdeliver %5ld\n",
			    rip->rif_stats.rifs_snotdeliver);

		p = bprintf(p, end, "upcall   %5ld ",
			    rip->rif_stats.rifs_upcall);
		p = bprintf(p, end, "flushed    %5ld\n",
			    rip->rif_stats.rifs_flushed);
	}
#endif /* STATISTICS */

	return p - begin;
}
#endif /* SMALL_KERNEL */

