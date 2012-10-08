/*	@(#)fragment.c	1.11	96/02/27 14:04:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "sys/flip/fragment.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "machdep.h"
#include "sys/proto.h"

/*
 * This module implements fair fragmentation. It enforces rate control between
 * two locations using credits. For each destination it keeps a pair of credit
 * counters <mycredit, scredit>. For each network packet that is sent to
 * the destination, it decreases mycredit. If mycredit becomes zero, it
 * queues all packets for the destination until it receives new credits from
 * the destination. If new credits do not arrive within a certain time, it
 * asks for new credits. In this way communication continues even if
 * credits are lost (when after a number of requests for credits, no reply is
 * coming back the destination is assumed to have failed and the message is
 * discarded).
 *
 * For each packet coming from the destination, it increases scredit.
 * When scredit reaches a certain threshold, it sends to the destination
 * new credits.
 *
 * To avoid wasting bandwidth on sending of credits, scredits should be
 * piggybacked on packets that are going to the destination. It is up
 * to the link-layer to implement this.
 *
 * Credits are distributed fairly among flip entities that run on the same
 * location. If, for example, two flip entities decide to send a 1Mbyte
 * message to different flip entities on the same destination location, the
 * messages will arrive at the destination intermixed. In this way short 
 * messages do not have to wait until a long messages has been sent. Messages
 * that are sent to the same flip address will be sent one after the other.
 *
 * We do not implement any flow control for multidata packets, because
 * we do not know how to do this.
 */

#define MAXLOCIDLE		300000	/* (ms) */
#define MAXFLIPIDLE		100000 	/* (ms) */
#define CREDITTIMER    		2000	/* (ms) */
#define MAXTRIAL		5

#define LOGHASH		5
#define MAXHASH		(1 << LOGHASH)
#define HASHMASK	(MAXHASH-1)


#define MON_EVENT(str)  DPRINTF(0, ("%s\n", str))


#define hash(l)		(((l)->u_loc.fl_bytes[3] + (l)->u_loc.fl_bytes[4] +\
			  (l)->u_loc.fl_bytes[5]) & HASHMASK)
#define MIN(a,b)	((a<b) ? a : b)

/* To provide fair scheduling, the module keeps per network (device) a ring
 * of locations. Per location a ring of flip addresses and per flip address
 * a queue of packets.
 */

/* Structure per flip address. */
typedef struct adrtype {
	adr_t		a_dst;	 /* flip address */
	pkt_p		a_first; /* first packet */
	pkt_p		a_last;	 /* pointer to last packet in the queue */
	int		a_idle;  /* idle time for flip address */
	struct adrtype 	*a_next; /* pointer to next flip address in ring */
	struct adrtype 	*a_prev; /* pointer to previous address in ring */
} fadr_t, *fadr_p;


/* Strucure per location. The module keeps per location a ring of flip 
 * addresses. In this ring it keeps pointer to the next flip address to
 * which a packet may be sent. This pointer moves round-robin through
 * the ring.
 */
typedef struct loctype {
	location 	l_location; 	/* location */
	int		l_timer; 	/* timer on mycredit */
	int		l_trial; 	/* #retrial before giving up */
	fc_cnt_t	l_mycredit;	/* my credits for the location */
	fc_cnt_t	l_scredit;	/* credits from the remote loc */
	fc_cnt_t	l_acredit; 	/* arrived credits */
	int		l_npkt;		/* #packets in queues for loc */
	int		l_idle;		/* idle time for location */
	struct loctype  *l_hlink;       /* next loc with same hash value */
	struct loctype	*l_next;	/* next loc in the ring */
	struct loctype	*l_prev;	/* previous loc in the ring */
	fadr_p		l_adrnext;	/* pointer in flip ring */
} loc_t, *loc_p;


/* Structure per device (flip-network). It keeps a ring of locations and
 * a pointer to the next location to which a packet may be sent. This
 * pointer moves round-robin through the location ring. Location addresses
 * are hashed.
 */
typedef struct ff_ntw {
	int	n_used;		/* used? */
	pool_t  n_pool;		/* pool with packets. */
	pkt_p	n_pkt;
	char 	*n_pktdata;
	loc_p	*n_hash;	/* hash table on location addresses */
	loc_p 	n_next;		/* next location that may send a packet */
	int	n_npkt;		/* #packets in queues for this network */
	void    (*n_request)(); /* ask for more credits */
	void 	(*n_cleanup)();	/* could not send pkt; cleanup */
	int	n_devno;	/* device number */
	long 	n_mtu;		/* max transmission unit for this device */
	int	n_nsndpkt;	/* the max number of outgoing pkts in a blast */
	int	n_nrcvpkt;	/* the max number of incoming pkt in a blast */
	int	n_credit;	/* total number per loc in one blast */
	int	n_threshold;	/* number of credits before sending them back */
} ff_ntw_t, *ff_ntw_p;


extern uint16 ff_pktsize;
extern uint16 ff_maxnloc;
extern uint16 ff_maxnfadr;

static ff_ntw_p ff_ntwtab;		/* table with device structures */
static int nnetwork;		/* number of devices */
static fadr_p freefadr;		/* list of free address structures */
static loc_p freeloc;		/* list of free location structures */

static loc_p lastsrcloc, lastdstloc;	/* remember the last location used */

static void purge_fadrs( /* int enough */ );

/* Fill new flip address structure. */
static fadr_p fadr_make(pkt, adr)
    pkt_p pkt;
    adr_p adr;
{
    fadr_p fa;
    
    if(freefadr == 0) {
	/* Ran out of fadrs; try to purge unused ones.  A quarter of the ones
	 * allocated initialially should be enough for a while.
	 */
	MON_EVENT("purge fadrs");
	purge_fadrs((int) (ff_maxnfadr / 4));
    }

    if (freefadr == 0) {
#ifndef SMALL_KERNEL
	ff_dump((char *) 0, (char *) 0);
#endif
	panic("fadr_make: out of fadr");
    }
    fa = freefadr;
    freefadr = fa->a_next;
    fa->a_next = fa;
    fa->a_prev = fa;
    fa->a_first = pkt;
    fa->a_last = pkt;
    fa->a_dst = *adr;
    fa->a_idle = 0;
    pkt->p_admin.pa_next = 0;
    return(fa);
}


/* Fill new location structure. */
static loc_p loc_make(loc)
    location *loc;
{
    loc_p lp;
    
    lp = freeloc;
    if (lp == 0) {
	/* We could try to do a purge in this case, but this situation
	 * really means that ff_maxnloc is configured too small.
	 */
	DPRINTF(0, ("loc_make: out of locations\n"));
	return 0;
    }
    freeloc = lp->l_next;
    lp->l_location = *loc;
    lp->l_adrnext = 0;
    lp->l_scredit = 0;
    lp->l_timer = -1;
    lp->l_adrnext = 0;
    lp->l_idle = 0;
    lp->l_acredit = 0;
    lp->l_trial = 0;
    lp->l_npkt = 0;
    lp->l_next = lp;
    lp->l_prev = lp;
    lp->l_hlink = 0;
    return lp;
}


/* Remove a location from the location ring. If the location has an address
 * remove, remove the address ring.
 */
static void ff_remove(ntw, lp)
    ff_ntw_p ntw;
    loc_p lp;
{
    loc_p *hp, lq;
    fadr_p fa, fp;
    pkt_p nextpkt, pkt;
    static location loc;
    
    if(lastdstloc != 0 && LOC_EQUAL(&lp->l_location,
				    &lastdstloc->l_location)) { 
	/* invalidate last destination location. */
	lastdstloc = 0;
    }
    if(lastsrcloc != 0 && LOC_EQUAL(&lp->l_location,
				    &lastsrcloc->l_location)) { 
	/* invalidate last source location. */
	lastsrcloc = 0;
    }
    /* get lp out of the hash table */
    hp = &(ntw->n_hash[hash(&lp->l_location)]);
    if(*hp == lp) *hp = lp->l_hlink;
    else {
	for(lq = *hp; lq != 0 && lq->l_hlink != lp; lq = lq->l_hlink) ;
	assert(lq->l_hlink == lp);
	lq->l_hlink = lp->l_hlink;
    }
    loc = lp->l_location;	/* remember location */
    /* now get lp out of the ring */
    for(lq = ntw->n_next; lq->l_next != lp ;lq = lq->l_next) ;
    if(lq == lp) {
	ntw->n_next = 0;
    }
    else {
	lq->l_next = lp->l_next;
	lp->l_next->l_prev = lq;
	if(ntw->n_next == lp) ntw->n_next = lp->l_next;
    }
    lp->l_next = freeloc;
    freeloc = lp;
    /* remove all fadr from lp */
    for(fa = lp->l_adrnext; fa != 0; fa = fp) {
	/* remove packets */
	nextpkt = 0;
	for(pkt = fa->a_first; pkt != 0; pkt = nextpkt) {
	    ntw->n_npkt--;
	    assert(ntw->n_npkt >= 0);
	    nextpkt = pkt->p_admin.pa_next;
	    if(ntw->n_cleanup) (*ntw->n_cleanup)(&loc, pkt);
	    else pkt_discard(pkt);
	}
	fa->a_first = 0;
	fa->a_last = 0;
	fp = fa->a_next;
	fa->a_next = freefadr;
	freefadr = fa;
	if(fp == lp->l_adrnext) break;
    }
}


#ifdef RVDP_DEBUG
static int
req_credit(ntw, lp)
#else
static void req_credit(ntw, lp)
#endif
    ff_ntw_p ntw;
    loc_p lp;
{
    pkt_p pkt;
    fc_hdr_p fc;

#ifdef RVDP_DEBUG
	if (ntw == 0) return -10;
    if(ntw->n_request == 0) return 0;
#else
    if(ntw->n_request == 0) return;
#endif
    PKT_GET(pkt, &ntw->n_pool);
    if(pkt == 0) {
	DPRINTF(0, ("WARNING: req_credit: out of memory\n"));
#ifdef RVDP_DEBUG
	return 0;
#else
	return;
#endif
    }
    proto_init(pkt);
    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
    fc->fc_type = FC_REQCREDIT;
    proto_fix_header(pkt);
    (*ntw->n_request)(ntw->n_devno, &lp->l_location, pkt);
#ifdef RVDP_DEBUG
	return 0;
#endif
}

static int
sweep_location(lp, inc_flipidle, max_flipidle)
loc_p lp;
int inc_flipidle;
int max_flipidle;
/* Increment the idle times for the flip addresses involved with lp.
 * Remove the ones that have no packets queued for it, and that have been
 * idle for at least max_flipidle.  Return the number of faddrs removed.
 */
{
    register fadr_p fa, hfa;
    register int nremoved = 0;

    for(fa = lp->l_adrnext; fa != 0 ; fa = hfa) {
	if (!fa->a_first)
	    fa->a_idle += inc_flipidle;
	hfa = fa->a_next;
	if(fa->a_idle >= max_flipidle && !fa->a_first)
	{
	    if(fa == hfa) {
		hfa = 0;
		lp->l_adrnext = 0;
	    }
	    else {
		fa->a_next->a_prev = fa->a_prev;
		fa->a_prev->a_next = fa->a_next;
		if(fa == lp->l_adrnext)
		    lp->l_adrnext = hfa;
	    }

	    fa->a_next = freefadr;
	    freefadr = fa;
	    nremoved++;
	}
	if(hfa == lp->l_adrnext) break;
    }

    return(nremoved);
}

static void
purge_fadrs(enough)
int enough;
{
    /* For some reason we ran out of faddrs.  Possibly we are talking to
     * many different processes, and ff_sweep() isn't quick enough in
     * removing unused faddrs.
     */
    register ff_ntw_p ntw;
    register loc_p lp;
    register int npurged = 0;
    
    for(ntw = ff_ntwtab; ntw < ff_ntwtab + nnetwork; ntw++) {
	for(lp = ntw->n_next; lp != 0; lp = lp->l_next) {
	    /* Don't increase the idle times, but any fadr which has
	     * a_idle > 0 may be purged.
	     */
	    npurged += sweep_location(lp, 0, 1);
	    if (npurged >= enough) {
		return;
	    }

	    if(ntw->n_next == 0 || lp->l_next == ntw->n_next) break;
	}
    }
}

/* Sweep through the fragmentation data structures. If a flip address has
 * not been used for a period of time, remove it from the flip ring. If
 * a location has not been used for a period of time, remove if from the
 * location ring. If we have been waiting too long for new credits, give
 * ourselves new credits.
 */
static void ff_sweep(id)
    long id;
{
    register ff_ntw_p ntw;
    register loc_p lp, hlp;
    
    for(ntw = ff_ntwtab; ntw < ff_ntwtab + nnetwork; ntw++) {
	for(lp = ntw->n_next; lp != 0; lp = hlp) {
	    if(lp->l_timer > 0) {
		lp->l_timer -= id;
		if(lp->l_timer <= 0) {
		    lp->l_trial++;
		    lp->l_timer = CREDITTIMER;
#ifdef RVDP_DEBUG
		    if(lp->l_trial < MAXTRIAL) {
			if (req_credit(ntw, lp) == -10)
				printf("req_cred got 0 I gave %x\n", ntw);
		    }
#else
		    if(lp->l_trial < MAXTRIAL) req_credit(ntw, lp);
#endif
		    else {
			MON_EVENT("ff_sweep: no response on credit request");
			lp->l_idle = MAXLOCIDLE + 1;
		    }
		}
	    }

	    (void) sweep_location(lp, (int) id, MAXFLIPIDLE);

	    lp->l_idle += id;
	    hlp = lp->l_next;
	    if(lp->l_idle >= MAXLOCIDLE) {
		ff_remove(ntw, lp);	
	    }
	    if(ntw->n_next == 0 || hlp == ntw->n_next) break;
	}
    }
}


/* Try to find location loc, or allocate it when not found on network ntw.
 * Return the location pointer, or 0 if we cannot allocate a new one.
 */
static loc_p ff_loc_find(ntw, loc)
ff_ntw_p  ntw;
location *loc;
{
    loc_p lp;

    for (lp = ntw->n_hash[hash(loc)]; lp != 0; lp = lp->l_hlink) {
	if (LOC_EQUAL(&lp->l_location, loc)) {
	    break;
	}
    }

    if (lp == 0) {
	if ((lp = loc_make(loc)) != 0) {
	    lp->l_mycredit = ntw->n_credit;
	    lp->l_hlink = ntw->n_hash[hash(loc)];
	    ntw->n_hash[hash(loc)] = lp;
	    if (ntw->n_next == 0) {
		ntw->n_next = lp;
	    } else {
		lp->l_next = ntw->n_next;
		lp->l_prev = ntw->n_next->l_prev;
		ntw->n_next->l_prev->l_next = lp;
		ntw->n_next->l_prev = lp;
	    
	    }
	}
    }

    return lp;
}


/* Received cnt credits from location loc.
 * Return the number of packets that are queued for loc.
 */
int ff_rcv_credit(ntwno, loc, cnt, eventtype)
    int ntwno;
    location *loc;
    fc_cnt_t cnt;
    int eventtype;
{
    register loc_p lp;
    ff_ntw_p ntw = ff_ntwtab+ntwno;

    assert(ntwno >= 0 && ntwno < nnetwork);
    assert(ntw->n_used);
    lp = lastsrcloc;
    if (lp == 0 || !LOC_EQUAL(loc, &lp->l_location))  {
	if ((lp = ff_loc_find(ntw, loc)) == 0) {
	    return 0;
	} else {
	    lastsrcloc = lp;
	}
    }
    lp->l_idle = 0;
    lp->l_trial = 0;
    switch(eventtype) {
    case EV_ARRIVE:
	lp->l_acredit++;
	break;
    case EV_RELEASE:
	assert(lp->l_acredit > 0);
	lp->l_acredit--;
	lp->l_scredit++;
	break;
    case EV_ABSCREDIT:
	lp->l_mycredit = 0;
	break;
    }
    lp->l_mycredit += cnt;
    if (lp->l_mycredit > 0) lp->l_timer = -1;
    return(lp->l_npkt);
}


/* The caller could not piggyback the outstanding credits for loc. It asks
 * to send them in a credit message.
 */
pkt_p ff_snd_credit(ntwno, loc, abs)
    int ntwno;
    location *loc;
    int abs;
{
    ff_ntw_p ntw = ff_ntwtab + ntwno;
    pkt_p pkt;
    fc_hdr_p fc;
    fc_cnt_t credit;
    fc_type_t type;
    register loc_p lp;
    
    assert(ntwno >= 0 && ntwno < nnetwork);
    assert(ntw->n_used);
    lp = lastsrcloc;
    if (lp == 0 || !LOC_EQUAL(loc, &lp->l_location)) {
	if ((lp = ff_loc_find(ntw, loc)) == 0) {
	    return 0;
	} else {
	    lastsrcloc = lp;
	}
    }
    lp->l_idle = 0;
    if(abs) {
	lp->l_scredit = 0;
	type = FC_ABSCREDIT;
	if(ntw->n_credit <= (int) lp->l_acredit) credit = 0;
	else credit = ntw->n_credit - lp->l_acredit;
    } else {
	if((int) lp->l_scredit >= ntw->n_threshold) {
	    type = FC_CREDIT;
	    credit = lp->l_scredit;
	    lp->l_scredit = 0;
	}
	else return(0);
    }

    PKT_GET(pkt, &ntw->n_pool);
    if(pkt == 0) {
	DPRINTF(0, ("WARNING: ff_snd_credit: out of memory\n"));
	return(0);
    }
    proto_init(pkt);
    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
    fc->fc_type = type;
    fc->fc_cnt = credit;
    proto_fix_header(pkt);
    return(pkt);
}


/* 
 * Get from the front of the message a packet. 
 * Assumptions: all direct data is smaller than mtu.
 */
static int ff_getfragment(ntw, pkt, rpkt, uni)
    ff_ntw_p ntw;
    pkt_p pkt;
    pkt_p *rpkt;
    int *uni;
{
    pkt_p newpkt;
    flip_hdr *fh, *newfh;
    long virsize, dirsize, nbytes;
    
    PKT_GET(newpkt, &ntw->n_pool);
    if(newpkt == 0) {
	MON_EVENT("dev_pool is empty");
	return(0);
    }
    proto_init(newpkt);
    newpkt->p_contents.pc_offset = pkt->p_contents.pc_offset;
    proto_cur_header(fh, pkt, flip_hdr);

    dirsize = proto_cur_size(pkt);
    assert(dirsize <= ntw->n_mtu + sizeof(flip_hdr));
    proto_dir_append(newpkt, pkt_offset(pkt), dirsize);
    PROTO_LOOK_HEADER(newfh, newpkt, flip_hdr);
    assert(newfh);

    virsize = pkt->p_contents.pc_totsize - pkt->p_contents.pc_dirsize;
    virsize = MIN(ntw->n_mtu - dirsize + sizeof(flip_hdr), virsize);
    assert(virsize >= 0);
    if(virsize > 0) {
	proto_set_virtual(newpkt, pkt->p_contents.pc_dstype,
			  pkt->p_contents.pc_dsident,
			  pkt->p_contents.pc_virtual, virsize);
    }

    nbytes = dirsize + virsize - sizeof(flip_hdr);
    pkt->p_contents.pc_dirsize = sizeof(flip_hdr);
    pkt->p_contents.pc_totsize -= nbytes;
    pkt->p_contents.pc_virtual += virsize;
    newfh->fh_length = nbytes;
    fh->fh_offset += nbytes;
    fh->fh_length -= nbytes;
    *uni  = newfh->fh_type == FL_T_UNIDATA;
    *rpkt = newpkt;
    return 1;
}


/* Get a packet from the queues for location lp. */
static int ff_getpkt(ntw, lp, rpkt, uni)
    ff_ntw_p ntw;
    loc_p lp;
    pkt_p *rpkt;
    int *uni;
{
    register fadr_p fa;
    flip_hdr *fh;
    pkt_p pkt;
    
    for(fa = lp->l_adrnext; fa != 0; fa = fa->a_next) {
	if((pkt = fa->a_first) != 0) break;
	if(fa->a_next == lp->l_adrnext) return(0);
    }
    if(fa == 0) panic("ff_getpkt: internal inconsistency");
    fa->a_idle = 0;
    lp->l_adrnext = fa->a_next;
    proto_cur_header(fh, pkt, flip_hdr);
    if(fh->fh_length <= ntw->n_mtu) {
	/* a message of one packet */
	*uni  = fh->fh_type == FL_T_UNIDATA;
	*rpkt = pkt;
	ntw->n_npkt--;
	lp->l_npkt--;
	fa->a_first = fa->a_first->p_admin.pa_next;
	if(fa->a_first == 0) fa->a_last = 0;
	return(1);
    }
    return(ff_getfragment(ntw, pkt, rpkt, uni));
}


/* Get a packet for network ntwno. */
int ff_get(ntwno, loc, pkt, credit)
    int ntwno;
    location *loc;
    pkt_p *pkt;
    fc_cnt_t *credit;
{
    ff_ntw_p ntw = ff_ntwtab + ntwno;
    register loc_p lp;
    int uni;
    
    assert(ntwno >= 0 && ntwno < nnetwork);
    assert(ntw->n_used);
    if(ntw->n_npkt == 0) return(0);
    for(lp = ntw->n_next; lp != 0 ; lp = lp->l_next) {
	if(lp->l_mycredit > 0 && lp->l_adrnext != 0) {
	    if(ff_getpkt(ntw, lp, pkt, &uni)) {
		*loc = lp->l_location;
		lp->l_idle = 0;
		ntw->n_next = lp->l_next;
		if(uni) {
		    if(lp->l_npkt > 0) lp->l_timer = CREDITTIMER;
		    else lp->l_timer = -1;
		    lp->l_mycredit -= 1;
		    if(credit) {
			*credit = lp->l_scredit;
			lp->l_scredit = 0;
		    }
		} else if(credit) {
		    *credit = 0;
		}
		return(ntw->n_npkt+1);
	    }
	}
	if(lp->l_next == ntw->n_next) return(0);
    }
    panic("ff_get: could not find entry (BUG)\n");
    return(0);
}


/* Enqueue packet for location lp. */
static void ff_storepkt(lp, pkt)
    register loc_p lp;
    pkt_p pkt;
{
    register flip_hdr *fh;
    fadr_p fa;
    
    proto_cur_header(fh, pkt, flip_hdr);
    for(fa = lp->l_adrnext; fa != 0 ; fa = fa->a_next) {
	if(ADR_EQUAL(&fa->a_dst, &fh->fh_dstaddr)) {
	    fa->a_idle = 0;
	    if(fa->a_last == 0) {
		fa->a_last = pkt;
		fa->a_first = pkt;
		pkt->p_admin.pa_next = 0;
	    }
	    else {
		if(pkt->p_admin.pa_priority) {
		    pkt->p_admin.pa_next = fa->a_first;
		    fa->a_first = pkt;
		} 
		else {
		    fa->a_last->p_admin.pa_next = pkt;
		    fa->a_last = pkt;
		    pkt->p_admin.pa_next = 0;
		}
	    }
	    return;
	}
	if(fa->a_next == lp->l_adrnext) break;
    }

    /* fh_dstaddr not yet present at location lp */
    fa = fadr_make(pkt, &fh->fh_dstaddr);
    if(lp->l_adrnext == 0) {
	DPRINTF(1, ("empty flip ring\n"));
	lp->l_adrnext = fa;
    }
    else {
	MON_EVENT("add flip destination to ring");
	fa->a_prev = lp->l_adrnext;
	fa->a_next = lp->l_adrnext->a_next;
	lp->l_adrnext->a_next->a_prev = fa;
	lp->l_adrnext->a_next = fa;
    }
}



/* Enqueue a packet for network ntwno. If no packets are enqueued for
 * this network, and the packet fits in one network packet, this routine
 * returns to the caller 1. The caller can now send the packet immediately
 * off. It can also piggyback credit credits on the same packet.
 */
int ff_store(ntwno, pkt, loc, credit)
    int ntwno;
    pkt_p pkt;
    location *loc;
    fc_cnt_t *credit;
{
    register ff_ntw_p ntw = ff_ntwtab + ntwno;
    register loc_p lp;
    register flip_hdr *fh;
    
    assert(ntwno >= 0 && ntwno < nnetwork);
    assert(ntw->n_used);
    proto_cur_header(fh, pkt, flip_hdr);
    if (lastdstloc == 0 || !LOC_EQUAL(&lastdstloc->l_location, loc)) {
	if ((lp = ff_loc_find(ntw, loc)) == 0) {
	    DPRINTF(0, ("ff_store: packet dropped\n"));
	    return 0;
	} else {
	    lastdstloc = lp;
	}
    }
    lastdstloc->l_idle = 0;
    if(fh->fh_length <= ntw->n_mtu && ntw->n_npkt == 0 && lastdstloc->l_mycredit
       > 0) { 
	if(fh->fh_type == FL_T_UNIDATA) { /* only unidata pkts cost a credit */
	    lastdstloc->l_mycredit -= 1;
	    lastdstloc->l_timer = CREDITTIMER;
	    if(credit) *credit = lastdstloc->l_scredit;
	    lastdstloc->l_scredit = 0;
	}
	/* BUG: we should do something for multidata packets, but how??? */
	return(1);
    } else {
	ntw->n_npkt++;
	lastdstloc->l_npkt++;
	if(lastdstloc->l_timer < 0)
	    lastdstloc->l_timer = CREDITTIMER;
	ff_storepkt(lastdstloc, pkt);
	return(0);
    }
}


/* Heuristics for the default number of credits.  We cut NCREDIT off at 8
 * because the receiving kernel may have less resources than we have.
 */
#define NCREDIT(ntw)	((ntw->n_nrcvpkt >> 3) ? 8 : \
			 ((ntw->n_nrcvpkt >> 2) ? (ntw->n_nrcvpkt >> 2) : 1))
#define THRESHOLD(ntw)	((NCREDIT(ntw) <= 2 ) ? 1 : (NCREDIT(ntw) - 2))


void ff_newntw(ntw, devno, mtu, nrcvpkt, nsndpkt, ff_request, ff_cleanup)
    int ntw;
    int devno;
    f_size_t mtu;
    uint16 nrcvpkt;
    uint16 nsndpkt;
    void (*ff_request) _ARGS(( int devno, location *loc, pkt_p pkt ));
    void (*ff_cleanup) _ARGS(( location *loc, pkt_p pkt ));
{
    ff_ntw_p ntwp;
    
    DPRINTF(0, ("ff_newntw: ntw %d allocate device with %d pkts(%d)\n", ntw,
	   nsndpkt, ff_pktsize));
    assert(ntw >= 0 && ntw < nnetwork);
    ntwp = ff_ntwtab + ntw;
    ntwp->n_request = ff_request;
    ntwp->n_cleanup = ff_cleanup;
    assert(ntwp->n_request);
    ntwp->n_devno = devno;
    ntwp->n_mtu = mtu;
    assert(nrcvpkt >= 1);
    assert(nsndpkt >= 1);
    ntwp->n_nrcvpkt = nrcvpkt;
    ntwp->n_nsndpkt = nsndpkt;
    ntwp->n_credit = NCREDIT(ntwp);
    ntwp->n_threshold = THRESHOLD(ntwp);
    ntwp->n_pkt = (pkt_p) aalloc((vir_bytes)(sizeof(pkt_t) * nsndpkt), 0);
    ntwp->n_pktdata = aalloc((vir_bytes)(ff_pktsize * nsndpkt), 0);
    pkt_init(&ntwp->n_pool, (int) ff_pktsize, ntwp->n_pkt, (int) nsndpkt,
		ntwp->n_pktdata, (void (*)()) 0, 0L);
    ntwp->n_used = 1;
}


/* Init this module. */
int ff_init(nntw)
    uint16 nntw;
{
    ff_ntw_p ntw;
    fadr_p fa;
    loc_p lp;
    int i;
    
    nnetwork = nntw;
    ff_ntwtab = (ff_ntw_p) aalloc((vir_bytes)(nntw * sizeof(ff_ntw_t)), 0);
    for(ntw= ff_ntwtab; ntw < ff_ntwtab + nntw; ntw++) {
    	ntw->n_used = 0;
	ntw->n_next = 0;
	ntw->n_npkt = 0;
	ntw->n_request = 0;
	ntw->n_hash = (loc_p *) aalloc((vir_bytes)(MAXHASH * sizeof(loc_p)), 0);
	for(i=0; i < MAXHASH; i++) ntw->n_hash[i] = 0;
    }
    freefadr = (fadr_p) aalloc((vir_bytes)(ff_maxnfadr * sizeof(fadr_t)), 0);
    freeloc = (loc_p) aalloc((vir_bytes)(ff_maxnloc * sizeof(loc_t)), 0);
    for(fa = freefadr; fa < freefadr + ff_maxnfadr - 1; fa++) {
	fa->a_next = fa+1;
    }
    for(lp = freeloc; lp < freeloc + ff_maxnloc - 1; lp++) {
	lp->l_next = lp+1;
    }
    lastsrcloc = 0;
    lastdstloc = 0;
    sweeper_set(ff_sweep, 500L, 500L, 0);
}


/* Debug routines. */
#ifndef SMALL_KERNEL
static int ff_adr_dump(begin, end, lp)
    char *begin, *end;
    loc_p lp;
{
    char *p;
    fadr_p fa;
    pkt_p pkt;
    flip_hdr *fh;
    
    p = begin;
    for(fa = lp->l_adrnext; fa != 0 ; fa = fa->a_next) {
	p = bprintf(p, end, "    "); 
	p += badr_print(p, end, &fa->a_dst); 
	p = bprintf(p, end, " age %d ", fa->a_idle);
	for(pkt = fa->a_first; pkt != 0; pkt = pkt->p_admin.pa_next) {
	    proto_cur_header(fh, pkt, flip_hdr);
	    p = bprintf(p, end, " %d(%d, %d)", fh->fh_total, fh->fh_offset,
								fh->fh_length);
	}
	p = bprintf(p, end, "\n");
	if(fa->a_next == lp->l_adrnext) break;
    }
    return(p - begin);
}


int ff_dump(begin, end)
    char *begin, *end;
{
    char *p;
    ff_ntw_p ntw;
    loc_p lp;
    int i;
    
    p = bprintf(begin, end, "==============  ff dump ===============\n");
    for(ntw = ff_ntwtab; ntw < &ff_ntwtab[nnetwork]; ntw++) {
	if(!ntw->n_used) continue;
	p = bprintf(p, end, "ntw: %d(%d, %d, %d) npkt: %d\n", ntw-ff_ntwtab, 
		    ntw->n_mtu, ntw->n_credit, ntw->n_threshold, ntw->n_npkt);
	for(i = 0; i < MAXHASH; i++) {
	    for(lp = ntw->n_hash[i]; lp !=0 ; lp = lp->l_hlink) {
		p += bloc_print(p, end, &lp->l_location);
		p = bprintf(p, end, " : credit: (m %d, s %d, a %d)", 
			    lp->l_mycredit , lp->l_scredit, lp->l_acredit);
		p = bprintf(p, end, 
			    " npkt %d age %d time %d # %d h %d\n",
			    lp->l_npkt, lp->l_idle, lp->l_timer, lp->l_trial,
			    i); 
		p += ff_adr_dump(p, end, lp);
	    }
	}
    }
    p = bprintf(p, end, "========================================\n");
    return(p - begin);
}
#endif
