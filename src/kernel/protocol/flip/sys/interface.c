/*	@(#)interface.c	1.13	96/02/27 14:05:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This module implements the flip interface (the interface between the
 * host and the flip-box). Functions starting with "flip_" form the flip
 * interface; Functions starting with "int_" are upcalls from the flip-box
 * to this module.
 * When a msg is sent from the interface, this module uses the routing
 * table of the flip box to do routing. It would have been more cleanly
 * if this module would keep its own routing table, but this would
 * mean duplication of code and loss of performance.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "sys/flip/flstat.h"
#include "sys/flip/switch.h"
#include "routtab.h"
#include "interfac-prv.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "byteorder.h"
#include "machdep.h"
#include "sys/flip/fl_byteorder.h"
#include "sys/proto.h"
#include "sys/flip/measure.h"
#include "module/fliprand.h"

#ifdef UNIX
#define await(v, d)	uxint_await(v, d)
#define wakeup(v)	uxint_wakeup(v)
#define NO_FLIP_WAIT_SUPPORT
#endif

#define MON_EVENT(x)	DPRINTF(0, ("%s\n", (x)));

static int if_ntw;				/* network number for FLIP */
						/* interface. */
static intface_p intface;
static intface_p *intfaceindex;
static alist_p al_table, al_free;		/* address list */
static locate_p l_table, l_list, l_free;	/* locate list */
static adr_t nulladdr;

extern struct netswitch **flip_indexswitch;	/* index in network switch */
extern uint16 int_maxint;
extern f_hopcnt_t max_hops;

#define MANY_LOCATIONS	16	/* many locations to be located (group code) */

#ifdef STATISTICS
flstat_t flstat;			/* statistics */
#define INT_STINC(type)	(flstat.type++);
#else
#define INT_STINC(type)
#endif


/* An address is removed from the routing table. Invalidate cache. */
void int_invalidate(adr)
    adr_p adr;
{
    intface_p intf;

    for(intf = intface; intf < intface + int_maxint; intf++) {
	if(ADR_EQUAL(adr, &intf->if_lastdst)) {
	    intf->if_lastdst = nulladdr;
	}
    }
}


/* Remove locate entry l from locate list. */
static void stoplocate(stopl)
    locate_p stopl;
{
    locate_p k, l;

    k = 0;
    for(l = l_list; l != 0; ) {
	if(l == stopl) {
	    if(l == l_list) {	/* first? */
		assert(k == 0);
		l_list = l->l_next;
	    } else {
		assert(k);
		k->l_next = l->l_next;
	    }
	    return;
	}
    }
}


/* Remove entry (me, adr) from the locate list. */
static void removelocate(me, adr)
    intface_p me;
    adr_p adr;
{
    locate_p k, l, tmpl;
    pkt_p tmppkt;

    k = 0;
    for(l = l_list; l != 0; ) {
	if(l->l_intf == me && (adr == 0 || ADR_EQUAL(adr, &l->l_dst))) {
	    tmppkt = l->l_pkt;
	    tmpl = l;
	    if(k == 0) {
		l_list = l->l_next;
		l = l_list;
	    } else {
		k->l_next = l->l_next;
		l = k->l_next;
	    }
	    if(tmpl->l_state == SLEEPING) { /* wakeup thread */
		tmpl->l_state = FAILED;
		wakeup((event) &tmpl->l_locevent);
	    } else {
		if(tmppkt != 0) pkt_discard(tmppkt);
		tmpl->l_next = l_free; /* set in free list */
		l_free = tmpl;
	    }
	} else {
	    k = l;
	    l = l->l_next;
	}
    }
}


/* Remove address from interface entry. */
static void dequeue(alp, adr)
    alist_p *alp;
    adr_p adr;
{
    alist_p al, prev;

    for(prev = 0,  al = *alp; al != 0; ) {
	if(ADR_EQUAL(&al->al_address, adr)) {
	    if(prev == 0) *alp = al->al_next;
	    else prev->al_next = al->al_next;
	    al->al_next = al_free;
	    al_free = al;
	    if(prev == 0) al = *alp;
	    else al = prev->al_next;
	} else {
	    prev = al;
	    al = al->al_next;
	}
    }
}


/* Fill flip header. */
static flip_hdr *setflip(intf, msg, fl, da, sa, hopcnt, length, type)
    intface_p intf;
    pkt_p msg;
    f_flag_t fl;
    adr_p da, sa;
    f_hopcnt_t hopcnt;
    f_size_t length;
    f_type_t type;
{
    register flip_hdr *fh;

    PROTO_ADD_HEADER(fh, msg, flip_hdr);
    fh->fh_version = FLIP_VERSION;
    fh->fh_flags = fl;
    fh->fh_act_hop = 0;
    fh->fh_messid = intf->if_messid++;
    fh->fh_dstaddr = *da;
    fh->fh_srcaddr = *sa;
    fh->fh_type = type;
    fh->fh_length = fh->fh_total = length;
    fh->fh_offset = 0;
    f_setendian(fh);
    fh->fh_max_hop = hopcnt;
    return fh;
}


/* A message is destined for a local transport entry. Deliver it. */
static void deliverlocal(pkt, dst, da, src, length, messid)
    pkt_p pkt;
    intface_p dst;
    adr_p da, src;
    f_size_t length;
    f_msgcnt_t messid;
{
    if(dst->if_state == IF_USED && dst->if_receive != 0) 
	(*dst->if_receive)(dst->if_ident, pkt, da, src, messid, 0, length,
			   length, 0); 
    else
	pkt_discard(pkt);
}


/* Send a Locate packet. */
static void sndlocate(l)
    locate_p l;
{
    pkt_p pkt;
    adr_p src;

    src = l->l_intf->if_endpoint[l->l_ep];

    if((pkt = flip_pkt_acquire()) == 0) {
        MON_EVENT("sndlocate: out of packets");
	return;
    }
    proto_init(pkt);
    l->l_timer = l->l_deltatime;

    /* Send locate without security; we want to know if the address exits;
     * From the reply we can tell if it is reachable through a safe path.
     */
    (void) setflip(l->l_intf, pkt, (f_flag_t) 0, &l->l_dst, src, l->l_hopcnt,
			0L, FL_T_LOCATE);
    INT_STINC(fl_slocate);
    pktswitch(pkt, if_ntw, &l->l_intf->if_location);
}


/* Dst is not in the routing table. Try to locate it. */
static int locate(intf, pkt, t, uni, dst, src, length, hcnt, count, flags, ltime)
    intface_p intf;
    pkt_p pkt;
    int uni;
    int t;
    adr_p dst;
    int src;
    f_size_t length;
    f_hopcnt_t hcnt;
    uint16 count;
    int flags;
    interval ltime;
{
    locate_p l, l_same = 0;
    int sync = flags & FLIP_SYNC;
    int r;

    if(l_free == 0) return(FLIP_FAIL);
    l = l_free;
    l_free = l->l_next;

    assert(ltime > 0);
    l->l_timeout = ltime;
    l->l_deltatime = LOCTIMEOUT;
    l->l_state = 0;
    l->l_intf = intf;
    l->l_dst = *dst;
    l->l_cnt = count;
    l->l_flags = flags & ~FLIP_SYNC;
    l->l_trusted = t;
    l->l_hopcnt = hcnt;
    l->l_retrial = 0;
    l->l_pkt = pkt;
    l->l_unicast = uni;
    l->l_ep = src;
    l->l_length = length;

    if (!sync) {
	/* See if we are already locating this address */
	for (l_same = l_list; l_same != 0; l_same = l_same->l_next) {
	    if (ADR_EQUAL(&l_same->l_dst, dst)) {
		break;
	    }
	}
    }
    if (l_same != 0) {
	/* Put it on the tail of the list, so that the packets get sent
	 * in the correct order once the locate succeeds.
	 * A 'last' pointer would have been useful here.
	 */
	locate_p lp;

	l->l_next = 0;
	for (lp = l_same; lp->l_next != 0; lp = lp->l_next) {
	    /* skip */
	}
	lp->l_next = l;
    } else {
	/* New address, just add it in front */
	l->l_next = l_list;
	l_list = l;
    }
    if (l_same == 0) {
	sndlocate(l);
    } else {
	MON_EVENT("flip locate pending");
    }
    if(sync) {
	MON_EVENT("locate: Sleep until locate is finished");
	l->l_state = SLEEPING;
	if(await((event) &l->l_locevent, 0L)) {
	    MON_EVENT("signal during interface locate");
	    l->l_pkt = 0;
	    stoplocate(l);
	}
	if(l->l_state == SUCCESS) {
	    r = FLIP_OK;
	} else if(l->l_trusted && adr_count(&l->l_dst, (f_hopcnt_t *)0, NONTW,
					     UNSECURE) >= l->l_cnt) {
	    r = FLIP_UNSAFE;
	} else {
	    r = FLIP_TIMEOUT;
	}
	l->l_pkt = 0;
	l->l_next = l_free;	/* put l back in the free ist */
	l_free = l;
	return(r);
    }
    return(FLIP_OK);
}


/* A Locate packet has been received.  See whether somebody here has
 * registered the Destination Address, and if so, send a HereIs message
 * back.
 */
int_locate(pkt)
    register pkt_p pkt;
{
    struct intface *intf;
    flip_hdr *fh, *nfh;
    struct address_list *al;
    uint16 count = 0;
    pkt_p new;
    
    proto_cur_header(fh, pkt, flip_hdr);
    for (intf = intface; intf < &intface[int_maxint]; intf++) {
	if (intf->if_state == IF_FREE)
	    continue;
	for (al = intf->if_addrs; al != 0; al = al->al_next) {
	    /* Increase count only once per interface. */
	    if (ADR_EQUAL(&al->al_address, &fh->fh_dstaddr)) {
		proto_fix_header(pkt);
		if((new = flip_pkt_new(pkt)) != 0) {
		    DPRINTF(1, ("int_locate: send hereis\n"));
		    count = 1;
		    fh = proto_look_header(pkt, flip_hdr);
		    nfh = proto_look_header(new, flip_hdr);
		    nfh->fh_type = FL_T_HEREIS;
		    nfh->fh_max_hop = fh->fh_act_hop;
		    nfh->fh_length += FLIP_HEREIS_LENGTH;
		    nfh->fh_total += FLIP_HEREIS_LENGTH;
		    proto_dir_append(new, (char *) &count, sizeof(count));
		    proto_dir_append(new, (char *) &count, sizeof(count));
		    INT_STINC(fl_shereis);
		    pktswitch(new, if_ntw, &intf->if_location);
		    break;	/* go back to main loop */
		} else {
		    MON_EVENT("int_locate: out of packets");
		    fh = proto_look_header(pkt, flip_hdr);
		}
	    } 
	}
    }
    pkt_discard(pkt);
}


/* A HereIs packet arrived.
 */
int_hereis(pkt)
    register pkt_p pkt;
{
    flip_hdr *fh;
    locate_p k, l;
    adr_p src;
    uint16 incr, trusted_incr;
    uint16 cnt;
    
    proto_cur_header(fh, pkt, flip_hdr);
    proto_dir_getoff(pkt, (char *) &trusted_incr, sizeof(trusted_incr));
    proto_dir_getoff(pkt, (char *) &incr, sizeof(incr));
    assert(fh->fh_length == FLIP_HEREIS_LENGTH);
    assert(incr >= trusted_incr);
    k = 0;
    for(l = l_list; l != 0; ) {
	src = l->l_intf->if_endpoint[l->l_ep];

	if(l->l_trusted) cnt = trusted_incr;
	else cnt = incr;

	if (ADR_EQUAL(&l->l_dst, &fh->fh_dstaddr) &&
	    ADR_EQUAL(src, &fh->fh_srcaddr) && cnt >= l->l_cnt)
	{
	    if (!l->l_unicast) {
		/* Let the sweeper handle the multicast locates during its
		 * next round.
		 */
		DPRINTF(0, ("int_hereis: located %d dst (time left %d)\n",
			    cnt, l->l_timer));
		l->l_timer = -1;
		k = l;
		l = l->l_next;
	    } else {
		/* The destination is there; send the packet. */
		DPRINTF(1, ("int_hereis: successfull locate; send packet\n"));
		l->l_timer = -1;
		if(l == l_list) l_list = l->l_next;
		else k->l_next = l->l_next;
		if(l->l_state == SLEEPING) {
		    l->l_state = SUCCESS;
		    wakeup((event) &l->l_locevent);
		} else {
		    if (l->l_unicast) {
			(void) flip_unicast(l->l_intf - intface, l->l_pkt,
					    l->l_flags, &l->l_dst, l->l_ep,
					    l->l_length, (interval) 0);
		    }
#ifndef NOMULTICAST
		    else {
			(void) flip_multicast(l->l_intf - intface, l->l_pkt,
					      l->l_flags,
					      &l->l_dst, l->l_ep, l->l_length,
					      l->l_cnt, (interval) 0);
		    }
#endif
		    l->l_pkt = 0;
		    l->l_next = l_free;
		    l_free = l;
		}
		if(k == 0) l = l_list;
		else l = k->l_next;
	    }
	} else {
	    k = l;
	    l = l->l_next;
	}
    }
    pkt_discard(pkt);
}


/* A NotHere packet has been received.  Deliver it to the Destination
 * if it's still here. If FL_F_UNREACHABLE flag is set in the packet it
 * means that the destination probabably exits but is not reachable through
 * trusted networks. In this case, set the flags to FLIP_UNTRUSTED.
 *
 * The following describes a scenario for a nothere packet with FL_F_UNREACHABLE
 * set. A unidata packet with FL_F_SECURITY is set is sent in the direction of
 * the destination. One of the gateways does not have a trusted network to
 * forward the packet on and sends it back as a untrusted packet with
 * FL_F_UNREAHABLE set. One of the gateways on the way back has an alternative
 * route to destination. It turns the packet back into a unidata packet
 * (FL_F_UNREACHABLE is still set). One of the gateways on the alternative route
 * does not have any forwarding info at all for the destination address. So, it
 * turns the packet in a nothere packet and sends it back. If none of the
 * gateways has another alternative route, the packet will get back in this
 * piece of code.
 */
int_nothere(me, pkt)
    intface_p me;
    register pkt_p pkt;
{
    flip_hdr *fh;
    adr_t dst;
    f_size_t o, l, t;
    f_msgcnt_t m;
    int f = 0;
    
    proto_cur_header(fh, pkt, flip_hdr);
    removelocate(me, &fh->fh_dstaddr);
    dst = fh->fh_dstaddr;
    m = fh->fh_messid;
    o = fh->fh_offset;
    l = fh->fh_length;
    t = fh->fh_total;
    if(FL_F_UNREACHABLE & fh->fh_flags) {
	assert(FL_F_SECURITY & fh->fh_flags);
	f |= FLIP_UNTRUSTED;
    } else {
	f |= FLIP_NOTHERE;
    }
    proto_remove_header(pkt);
    if(me->if_notdeliver != 0)
    	(*me->if_notdeliver)(me->if_ident, pkt, &dst, m, o, l, t, f);
    else
	pkt_discard(pkt);
}


/* A Untrusted packet has been received.  Deliver it to the Destination
 * if it's still here.
 */
int_untrusted(me, pkt)
    intface_p me;
    register pkt_p pkt;
{
    flip_hdr *fh;
    adr_t dst;
    f_size_t o, l, t;
    f_msgcnt_t m;
    
    proto_cur_header(fh, pkt, flip_hdr);
    removelocate(me, &fh->fh_dstaddr);
    dst = fh->fh_dstaddr;
    m = fh->fh_messid;
    o = fh->fh_offset;
    l = fh->fh_length;
    t = fh->fh_total;
    proto_remove_header(pkt);
    if(me->if_notdeliver != 0)
    	(*me->if_notdeliver)(me->if_ident, pkt, &dst, m, o, l, t,
			     FLIP_UNTRUSTED);
    else
	pkt_discard(pkt);
}


/* This routine is called from the packet switch.  It has routed a
 * packet to the interface module.  Call an appropriate routine.
 */
void int_send(dev, pkt, loc)
    int dev;
    pkt_p pkt;
    location *loc;
{
    register flip_hdr *fh;
    static adr_t src, dst;
    intface_p intf;
    f_size_t o, l, t;
    f_msgcnt_t m;
    int f;
    
    assert(dev == 0);
    assert(!LOC_NULL(loc));
    proto_cur_header(fh, pkt, flip_hdr);
    if (fh == 0) {
	pkt_discard(pkt);
	return;
    }
    switch (fh->fh_type) {
    case FL_T_LOCATE:	
	INT_STINC(fl_rlocate);
	int_locate(pkt);
	break;
    case FL_T_HEREIS:	
	INT_STINC(fl_rhereis);
	int_hereis(pkt);	

	break;
    case FL_T_UNIDATA:	
    case FL_T_MULTIDATA:	
#ifdef STATISTICS
	if(fh->fh_type == FL_T_UNIDATA)	{
	    INT_STINC(fl_runi);
	} else {
	    INT_STINC(fl_rmulti);
	}
#endif
	intf = * (intface_p *) loc;
	assert(intf >= intface && intf < intface + int_maxint);
	src = fh->fh_srcaddr;
	dst = fh->fh_dstaddr;
	m = fh->fh_messid;
	o = fh->fh_offset;
	l = fh->fh_length;
	t = fh->fh_total;
	f = (FL_F_UNSAFE & fh->fh_flags) ? FLIP_UNTRUSTED: 0;
	proto_remove_header(pkt);
	END_MEASURE(flip_rcv);
	if(intf->if_receive != 0) 
	    (*intf->if_receive)(intf->if_ident, pkt, &dst, &src,
							m, o, l, t, f); 
	else
	    pkt_discard(pkt);
	break;
    case FL_T_NOTHERE:	
	INT_STINC(fl_rnothere);
	intf = * (intface_p *) loc;

	assert(intf >= intface && intf < intface + int_maxint);
	int_nothere(intf, pkt);	
	break;
    case FL_T_UNTRUSTED:
	INT_STINC(fl_runtrusted);
	intf = * (intface_p *) loc;
	assert(intf >= intface && intf < intface + int_maxint);
	int_untrusted(intf, pkt);
	break;
    default:		
	pkt_discard(pkt);
    }
}



/* This routine is called from the packet switch.
 */
/*ARGSUSED*/
static void int_broadcast(dev, pkt)
    int dev;
    pkt_p pkt;
{
    intface_p intf;
    pkt_p new;
    
    assert(pkt);
    proto_fix_header(pkt);
    for(intf = intface; intf < intface + int_maxint; intf++) {
	if(intf->if_state == IF_USED && intf->if_broadcast) {
	    new = flip_pkt_new(pkt);
	    if(new != 0) {
		(void) proto_look_header(new, flip_hdr); /* for int_send ... */
		int_send(0, new, &intf->if_location);
	    } else
		MON_EVENT("int_broadcast: out of packets");
	}
    }
    pkt_discard(pkt);
}


/* A transport entity registers it's existence here. 
 */
flip_init(ident, receive, notdeliver)
    long ident;
    void (*receive)();
    void (*notdeliver)();
{
    struct intface *intf;
    int i;
    
    for (intf = intface; intf < &intface[int_maxint]; intf++)
	if (intf->if_state == IF_FREE) {
	    intf->if_state = IF_USED;
	    intf->if_messid = 0;
	    intf->if_broadcast = 0;
	    intf->if_ident = ident;
	    intf->if_receive = receive;
	    intf->if_notdeliver = notdeliver;
	    * (intface_p *) &intf->if_location = intf;
	    intf->if_addrs = 0;
	    for(i=0; i < MAXEP; i++) intf->if_endpoint[i] = 0;
	    return intf - intface;
	}
    return FLIP_FAIL;
}


/* Close transport entity. */
flip_end(ifno)
    int ifno;
{
    struct intface *me;
    struct address_list *al;
    
    me = intfaceindex[ifno];
    assert(me >= intface && me < intface + int_maxint);
    if (me->if_state == IF_USED) {
	me->if_state = IF_FREE;
	me->if_notdeliver = 0;
	me->if_receive = 0;
	me->if_broadcast = 0;
	removelocate(me, (adr_p) 0);
	while ((al = me->if_addrs) != 0) {
	    DPRINTF(1, ("flip_end: adr_remove\n"));
	    adr_remove(&al->al_address, if_ntw, &me->if_location, 1, NONTW);
	    me->if_addrs = al->al_next;
	    al->al_next = al_free;
	    al_free = al;
	}
	return 0;
    }
    return FLIP_FAIL;
}



/* Register an address for a transport entity.
 */
flip_register(ifno, adr)
    int ifno;
    adr_p adr;
{
    struct intface *me;
    struct address_list *al;
    adr_p *addr;
    int ep = -1;
    
    if(ifno < 0 || ifno > (int) int_maxint) return(FLIP_FAIL);
    me = intfaceindex[ifno];
    assert(me >= intface && me < intface + int_maxint);
    if(me->if_state == IF_FREE) return(FLIP_FAIL);
    if((al = al_free) == 0) return(FLIP_FAIL);
    al_free = al->al_next;
    if(ADR_NULL(adr)) al->al_address = *adr;
    else flip_oneway(adr, &al->al_address);
    al->al_next = me->if_addrs;
    me->if_addrs = al;
    if(ADR_NULL(adr)) {
	me->if_broadcast = 1;
    } else {
	adr_install(&al->al_address, if_ntw, &me->if_location, 0, LOCAL,
		    SECURE);
#ifdef SPEED_HACK
	eth_register_adr(&al->al_address, me);
#endif
    }
    for(addr = me->if_endpoint; addr < me->if_endpoint+MAXEP; addr++) {
	if(*addr == 0) {
	    *addr = &al->al_address;
	    ep = addr - me->if_endpoint;
	    break;
	}
    }
    if(ep == -1) {
	DPRINTF(1, ("flip_register: adr_remove\n"));
	adr_remove(&al->al_address, if_ntw, &me->if_location, 1, NONTW);
	dequeue(&me->if_addrs, &al->al_address);
	return(FLIP_FAIL);
    } else return ep;
}


/* Unregister an address for a transport entity. */
flip_unregister(ifno, ep)
    int ifno;
    int ep;
{
    adr_p adr;
    intface_p me;
    alist_p al;
    
    if(ifno >= (int) int_maxint || ifno < 0) return(FLIP_FAIL);
    me = intfaceindex[ifno];
    assert(me >= intface && me < intface + int_maxint);
    if(me->if_state == IF_FREE) return(FLIP_FAIL);
    if(ep < 0 || ep >= MAXEP) return(FLIP_FAIL);
    adr = me->if_endpoint[ep];
    if(adr == 0) return(FLIP_FAIL);
    if(ADR_NULL(adr)) me->if_broadcast = 0;
    dequeue(&me->if_addrs, adr);
    for(al = me->if_addrs; al != 0; al = al->al_next) 
	if(ADR_EQUAL(&al->al_address, adr)) 
	    break;
    if(al == 0 && !ADR_NULL(adr)) {
#ifdef SPEED_HACK
	eth_unregister_adr(adr);
#endif
	DPRINTF(1, ("flip_unregister: adr_remove\n"));
	adr_remove(adr, if_ntw, &me->if_location, 1, NONTW);
    }
    removelocate(me, adr);
    me->if_endpoint[ep] = 0;
    return(0);
}


/* Broadcast a flip message. */
flip_broadcast(ifno, pkt, ep, length, hopcnt)
    int ifno;
    pkt_p pkt;
    int ep;
    f_size_t length;
    f_hopcnt_t hopcnt;
{
    adr_p src;
    intface_p me, intf;
    pkt_p new;
    
    assert(pkt);
    if(ifno < 0 || ifno >= (int) int_maxint) {
	MON_EVENT("flip_broadcast: illegal ifno");
	return(FLIP_FAIL);
    }
    me = intfaceindex[ifno];
    assert(me >= intface && me < intface + int_maxint);
    assert(me->if_state == IF_USED);
    if(ep < 0 || ep >= MAXEP) {
	MON_EVENT("flip_broadcast: illegal ep");
	return(FLIP_FAIL);
    }
    src = me->if_endpoint[ep];
    assert(src != 0);
    
    for(intf = intface; intf < intface + int_maxint; intf++) {
	if(intf->if_state == IF_USED && intf->if_broadcast && intf != me) {
	    if((new = flip_pkt_new(pkt)) != 0)
	        deliverlocal(new, intf, &nulladdr, src, length, me->if_messid);
	}
    }
    if(hopcnt > 0) {
	(void) setflip(me, pkt, (f_flag_t) 0, &nulladdr, src, hopcnt, length,
		       FL_T_MULTIDATA);
	INT_STINC(fl_smulti);
	pktswitch(pkt, if_ntw, &me->if_location);
    } else pkt_discard(pkt);
    return(0);
}


#ifndef NOMULTICAST

static int
mc_look_still_locating(dst, cnt)
adr_p dst;
uint16 cnt;
{
    register locate_p l;

    /* See if we are still locating this address, even though we have
     * found enough destinations.  This can happen when we are waiting
     * for the hardware multicast acks.
     */
    for (l = l_list; l != 0; l = l->l_next) {
	if (ADR_EQUAL(&l->l_dst, dst) && l->l_cnt == cnt) {
	    if (l->l_timeout >= 0) {
		DPRINTF(1, ("mc_still_locating: yes (%d)\n", l->l_timeout));
		return 1;
	    } else {
		return 0;
	    }
	}
    }
    return 0;
}

#define mc_still_locating(dst, cnt) \
    (l_list != 0 && mc_look_still_locating(dst, cnt))

#ifdef __STDC__
static void
mc_stop_locating(adr_p dst, uint16 cnt)
#else
static void
mc_stop_locating(dst, cnt)
adr_p dst;
uint16 cnt;
#endif
{
    register locate_p l;

    /* stop the multicast locates for destination dst */
    for (l = l_list; l != 0; l = l->l_next) {
	if (ADR_EQUAL(&l->l_dst, dst) && l->l_cnt <= cnt) {
	    l->l_timeout = -1;
	}
    }
}

static int
mc_keep_locating(l)
locate_p l;
{
    /* Previously, the first few flip_multicast()s would always
     * be performed as (hardware) unicasts, as long as it was
     * not known for sure that all destinations were listening
     * to the hardware multicast address.  Here we will improve
     * that by moving the hardware multicast acknowledgement
     * fase into the generic locate procedure.  When data is
     * being sent to many members, this is a big win, since
     * it may also avoid many unnecessary retransmissions.
     */
    struct ntw_info *ni;
    struct netswitch *ns;
    int did_request;
    f_hopcnt_t hopcnt;
    uint16 ndst, mc_count;

    mc_count = 0;
    did_request = 0;
    ni = adr_lookup(&l->l_dst, &ndst, &hopcnt, 0);
    for (; ni != 0; ni = ni->ni_next) {
	ns = flip_indexswitch[ni->ni_network];

	if (ns->ns_setmc && ni->ni_nmcast < ni->ni_count &&
	    ni->ni_count >= ns->ns_nlocation)
	{
	    /* There may be a location who could possible listen to
	    ** a multicast address. Let's try it.
	    */
	    DPRINTF(0, ("mc_setup: %d < %d\n", ni->ni_nmcast, ni->ni_count));
	    (*ns->ns_setmc)(ns->ns_devno, ni, &l->l_dst);
	    did_request = 1;
	}
	mc_count += ni->ni_nmcast;
    }

    if (mc_count >= l->l_cnt || !did_request) {
	DPRINTF(0, ("int_sweep: %d destinations replied\n", mc_count));
	if (l->l_timeout != -1) {
	    /* The first one terminates all other locates looking
	     * for the same destinations.
	     */
	    mc_stop_locating(&l->l_dst, l->l_cnt);
	}
    } else {
	/* see if we have time to wait for all replies */
	if (l->l_timeout >= 500) {
	    DPRINTF(0, ("int_sweep: wait for %d mc replies\n",
			l->l_cnt - mc_count));
	    return 1;
	}
    }

    return 0;
}

/* Send a multicast message to dst. Make sure that at least n transport
 * entities listen to dst.
 */
int flip_multicast(ifno, pkt, flags, dst, ep, length, n, ltime)
    int ifno;
    pkt_p pkt;
    int flags;
    adr_p dst;
    int ep;
    f_size_t length;
    uint16 n;
    interval ltime;
{
    adr_p src;
    intface_p me;
    flip_hdr *fh;
    struct ntw_info *ni, *tmp;
    struct netswitch *ns;
    int last, remove;
    uint16 ndst;
    int safe;
    f_hopcnt_t hopcnt;
    int r;
#ifndef NO_FLIP_WAIT_SUPPORT
    int pkt_refcount;
#endif
    
    BEGIN_MEASURE(flip_snd);
    assert(pkt);
    if(ifno < 0 || ifno >= (int) int_maxint) {
	MON_EVENT("flip_multicast: illegal ifno");
	return(FLIP_FAIL);
    }
    me = intfaceindex[ifno];
    assert(me >= intface && me < intface + int_maxint);
    assert(me->if_state == IF_USED);
    if(ep < 0 || ep >= MAXEP) {
	MON_EVENT("flip_multicast: illegal ep");
	return(FLIP_FAIL);
    }
    if (ADR_NULL(dst)) {
        MON_EVENT("flip_multicast: destination addr null");
        return(FLIP_FAIL);
    }
    src = me->if_endpoint[ep];
    assert(src != 0);
    
    if(n <= 0) {
	MON_EVENT("flip_multicast: illegal n");
	return(FLIP_FAIL);
    }
    remove = 1;
    safe = flags & FLIP_SECURITY;
    ni = adr_lookup(dst, &ndst, &hopcnt, safe);
    if (flags & FLIP_INVAL || !ni || ndst < n || mc_still_locating(dst, n)) {
	if (ni == 0) {
	    MON_EVENT("flip_multicast: find route");
	} else {
	    DPRINTF(0, ("flip_multicast: find route (flag %x ndst %d n %d)\n",
			flags, ndst, n));
	}
	if(ltime <= 0) {
	    MON_EVENT("flip_multicast: no locate time, but address unknown");
	    return(FLIP_TIMEOUT);
	}
	r = locate(me, pkt, safe, 0, dst, ep, length,
		   (flags & FLIP_INVAL) ? max_hops + 1 : 1,
		   n, flags & (FLIP_SYNC | FLIP_SKIP_SRC), ltime);
	if(!((FLIP_SYNC & flags) && r == FLIP_OK)) {
	    return(r);
	}
	ni = adr_lookup(dst, &ndst, &hopcnt, safe);
	assert(ni && ndst >= n);
    }
    fh = setflip(me, pkt, (f_flag_t) (safe ? FL_F_SECURITY : 0), dst, src,
		hopcnt, length, FL_T_MULTIDATA);
#ifndef NO_FLIP_WAIT_SUPPORT
    if (flags & FLIP_SYNC) {
	if (pkt->p_contents.pc_virtual != 0) {
	    pkt_refcount = 1;
	    assert(pkt->p_admin.pa_wait == 0);
	    pkt->p_admin.pa_wait = &pkt_refcount;
	} else {
	    /* no need to wait when there is no virtual data */
	    flags &= ~FLIP_SYNC;
	}
    }
#endif
    ns = flip_indexswitch[ni->ni_network];
    INT_STINC(fl_smulti);
    if (ni->ni_nmcast >= ndst && ns->ns_weight <= hopcnt &&
	(!safe || ns->ns_trusted))
    {
	fh->fh_act_hop += hopcnt;
	if(!ns->ns_trusted) fh->fh_flags |= FL_F_UNSAFE;
	STINC(ns, ns_smultidata);
	STINC(flip_indexswitch[if_ntw], ns_rmultidata);
	(*ns->ns_send)(ns->ns_devno, pkt, &ni->ni_multicast);
    }
    else
    {
	/* tricky code for two reasons. First, avoid
	 * making copies of messages. Two, the routing table might change.
	 */
	STINC(flip_indexswitch[if_ntw], ns_rmultidata);
	do {
	    last = ni->ni_next == 0;
	    tmp = ni->ni_next;
	    remove = ni_multicast(ni, pkt, hopcnt, last,
		  (flags & FLIP_SKIP_SRC) ? &me->if_location : (location *) 0);
	    ni = tmp;
	} while (ni != 0);
	if(remove) pkt_discard(pkt);
    }
#ifndef NO_FLIP_WAIT_SUPPORT
    if (flags & FLIP_SYNC) {
	while (pkt_refcount > 0) {
	    if (await((event) &pkt_refcount, 0) < 0) {
	        DPRINTF(0, ("flip_multicast: await 0x%lx failed\n",
			    &pkt_refcount));
	    }
	}
    }
#endif
    return(0);
}
#endif /* NOMULTICAST */


/* Send a point-to-point message to dst. */
int flip_unicast(ifno, pkt, flags, dst, ep, length, ltime)
    int ifno;
    pkt_p pkt;
    int flags;
    adr_p dst;
    int ep;
    f_size_t length;
    interval ltime;
{
    adr_p src;
    intface_p me, intf;
    struct netswitch *ns;
    flip_hdr *fh;
    int r;
    int safe;
#ifndef NO_FLIP_WAIT_SUPPORT
    int pkt_refcount;
#endif
    
    BEGIN_MEASURE(flip_snd);
    assert(pkt);
    if(ifno < 0 || ifno >= (int) int_maxint) {
	MON_EVENT("flip_unicast: illegal ifno");
	return(FLIP_FAIL);
    }
    me = intfaceindex[ifno];
    assert(me >= intface && me < intface + int_maxint);
    assert(me->if_state == IF_USED);
    if(ep < 0 || ep >= MAXEP) {
	MON_EVENT("flip_unicast: illegal ep");
	return(FLIP_FAIL);
    }
    if (ADR_NULL(dst)) {
        MON_EVENT("flip_unicast: destination addr null");
        return(FLIP_FAIL);
    }
    src = me->if_endpoint[ep];
    assert(src != 0);
    
    safe = flags & FLIP_SECURITY;
    if(flags & FLIP_INVAL) {
	if(ltime <= 0) {
	    MON_EVENT("flip_unicast: no locate time, but address unknown");
	    return(FLIP_TIMEOUT);
	}
	me->if_lastdst = nulladdr;
	r = locate(me, pkt, safe, 1, dst, ep, length, max_hops + 1, 1, 
		   flags & FLIP_SYNC, ltime);
	if(!((flags & FLIP_SYNC) && r == FLIP_OK)) {
	    /* If asynchronous, or synchronous and locate failed, return. */
	    return(r);
	}
    }
    if(!ADR_EQUAL(&me->if_lastdst, dst) || (safe && !me->if_lastsafe)) {
	me->if_lastdst = *dst;
	if(adr_route(dst, max_hops, &me->if_lastntw, &me->if_lastloc, 
		      &me->if_lasthcnt, NONTW, safe) != ADR_OK) {
	    me->if_lastdst = nulladdr;
	    if(ltime <= 0) {
		MON_EVENT("flip_unicast: no locate time, but address unknown");
		return(FLIP_TIMEOUT);
	    }
	    r = locate(me, pkt, safe, 1, dst, ep, length, 1, 1, flags &
		       FLIP_SYNC, ltime);
	    if(!((flags & FLIP_SYNC) && r == FLIP_OK)) {
		/* If asynchronous, or synchronous and locate failed, return. */
		return(r);
	    }
	    if(adr_route(dst, max_hops, &me->if_lastntw, &me->if_lastloc, 
		      &me->if_lasthcnt, NONTW, safe) != ADR_OK) {
	        /* How is  this possible, we just located the bloody address! */
	        assert(0);
	     }
	}
	me->if_lastdst = *dst;
	me->if_lastsafe = safe;
    }
#ifndef NO_FLIP_WAIT_SUPPORT
    if (flags & FLIP_SYNC) {
	if (pkt->p_contents.pc_virtual != 0) {
	    pkt_refcount = 1;
	    assert(pkt->p_admin.pa_wait == 0);
	    pkt->p_admin.pa_wait = &pkt_refcount;
	} else {
	    /* no need to wait when there is no virtual data */
	    flags &= ~FLIP_SYNC;
	}
    }
#endif
    if(me->if_lasthcnt == 0) {
	intf = * (intface_p *) &me->if_lastloc;
	assert(intface <= intf && intf < intface + int_maxint);
	deliverlocal(pkt, intf, dst, src, length, me->if_messid++);
    } else {
	INT_STINC(fl_suni);
	ns = flip_indexswitch[me->if_lastntw];
	fh = setflip(me, pkt, (f_flag_t)(safe ? FL_F_SECURITY : 0), dst, src,
		     me->if_lasthcnt, length, FL_T_UNIDATA); 
	fh->fh_act_hop += ns->ns_weight;
	if(!ns->ns_trusted) fh->fh_flags |= FL_F_UNSAFE;
	STINC(ns, ns_sunidata);
	STINC(flip_indexswitch[if_ntw], ns_runidata);
	(*ns->ns_send)(ns->ns_devno, pkt, &me->if_lastloc);
    }
#ifndef NO_FLIP_WAIT_SUPPORT
    if (flags & FLIP_SYNC) {
	while (pkt_refcount > 0) {
	    if (await((event) &pkt_refcount, 0) < 0) {
	        DPRINTF(0, ("flip_unicast: await 0x%lx failed\n",
			    &pkt_refcount));
	    }
	}
    }
#endif
    return(0);
}


#ifndef SMALL_KERNEL
#ifdef STATISTICS
int int_statistics(begin, end)
    char *begin;
    char *end;
{
    char *p;
    
    p = bprintf(begin, end, "======= FLIP interface statistics ===========\n");
    p = bprintf(p, end, "slocate    %7ld ",  flstat.fl_slocate);
    p = bprintf(p, end, "shereis    %7ld ",  flstat.fl_shereis);
    p = bprintf(p, end, "snothere   %7ld ",  flstat.fl_snothere);
    p = bprintf(p, end, "sunidata   %7ld\n",  flstat.fl_suni);
    p = bprintf(p, end, "smultidata %7ld\n",  flstat.fl_smulti);
    p = bprintf(p, end, "rlocate    %7ld ",  flstat.fl_rlocate);
    p = bprintf(p, end, "rhereis    %7ld ",  flstat.fl_rhereis);
    p = bprintf(p, end, "rnothere   %7ld ",  flstat.fl_rnothere);
    p = bprintf(p, end, "runidata   %7ld\n",  flstat.fl_runi);
    p = bprintf(p, end, "rmultidata %7ld ",  flstat.fl_rmulti);
    p = bprintf(p, end, "runtrusted %7ld\n",  flstat.fl_runtrusted);
    return p - begin;
}
#endif


int int_dump(begin, end)
    char *begin;
    char *end;
{
    struct intface *intf;
    struct address_list *al;
    char *p = begin;
    locate_p l;
    
    p = bprintf(p, end, "====== interface module ======\n");
    for(intf = intface; intf < &intface[int_maxint]; intf++)
	if(intf->if_state == IF_USED) {
	    p = bprintf(p, end, "int %d%s broadcast, receive=%x, notdeliver=%x\n(\n",
		    intf - intface, intf->if_broadcast ? "" : " no",
		    intf->if_receive, intf->if_notdeliver);
	    for (al = intf->if_addrs; al != 0; al = al->al_next){
		p = bprintf(p, end, "       "); 
		p += badr_print(p, end, &al->al_address);
		p = bprintf(p, end, "\n");
	    }
	    p = bprintf(p, end, ")\n");
	}
    for(l = l_list; l != 0; l = l->l_next) {
	p += badr_print(p, end, &l->l_dst);
	p = bprintf(p, end, " hcnt %d retrial %d delta %d cnt %d\n",
		l->l_hopcnt, l->l_retrial, l->l_deltatime, l->l_cnt);
    }
    p = bprintf(p, end, "==============================\n");
    return(p - begin);
}
#endif

static int onlist(mylocate)
    locate_p mylocate;
{
    locate_p l;

    for(l = l_list; l != mylocate; l = l->l_next) {
	if(ADR_EQUAL(&l->l_dst, &mylocate->l_dst)) {
	    return(1);
	}
    }
    return(0);
}


/* Sweep through the list of locate structures. For each entry increase
 * the hopcount and send a new locate. If the hopcount reaches max_hops,
 * give the destination some more time to reply; on each timeout multiply
 * by two. If l_timeout runs out stop locating.
 */
static void int_sweep(id)
    long id;
{
    locate_p k;
    locate_p l;
    adr_t dst;
    void (*notdeliver)();
    long ident;
    pkt_p pkt;
    int f = 0;
    
    k = 0;
    for(l = l_list; l != 0; ) {
	l->l_timer -= id;
	if(l->l_timer > 0 || onlist(l)) {
	    k = l;
	    l = l->l_next;
	} else {		/* timeout */
	    l->l_timeout -= l->l_deltatime;
	    if(l->l_hopcnt >= max_hops) l->l_deltatime *= 2;
	    else if (l->l_cnt > MANY_LOCATIONS && l->l_hopcnt > 2) {
		/* Also increase the deltatime in case many locations
		 * are to be found (e.g. groups with many members).
		 */
	        DPRINTF(0, ("int_sweep: %d of %d locations found\n",
			    adr_count(&l->l_dst, (f_hopcnt_t *)0, NONTW,
				      l->l_trusted),
			    l->l_cnt));
	        if (l->l_deltatime < 1000) {
		    l->l_deltatime *= 2;
	            DPRINTF(0, ("int_sweep: deltatime now %d\n",
			        l->l_deltatime));
		}
	    }
	    l->l_timer = -1;
	    l->l_retrial++;
	    if(l->l_retrial >= MAXRETRIAL) {
		l->l_retrial = 0;
		if(l->l_hopcnt < max_hops) l->l_hopcnt++;
	    }
	    if (adr_count(&l->l_dst, (f_hopcnt_t *)0, NONTW, l->l_trusted)
		>= l->l_cnt) 
	    {
#ifndef NOMULTICAST
		if (!l->l_unicast && mc_keep_locating(l)) {
		    /* Still waiting for mc acks; skip to the next.
		     * Reset l->l_deltatime, since it may be increased
		     * a lot during the FLIP locate procedure.
		     * We don't need that here.
		     */
		    l->l_timer = l->l_deltatime = 100;
		    k = l;
		    l = l->l_next;
		} else
#endif
		{
		    /* The destination is there; send the packet. */
		    DPRINTF(1, ("int_sweep: enough locations; send packet\n"));
		    if(k == 0) l_list = l->l_next; /* remove from list */
		    else k->l_next = l->l_next;
		    if(l->l_state == SLEEPING) {
			l->l_state = SUCCESS;
			wakeup((event) &l->l_locevent);
		    } else {
			if(l->l_unicast) 
			    (void)flip_unicast(l->l_intf - intface, l->l_pkt,
						l->l_flags, &l->l_dst, l->l_ep,
						l->l_length, (interval) 0);
#ifndef NOMULTICAST
			else 
			    (void)flip_multicast(l->l_intf - intface, l->l_pkt,
						 l->l_flags, &l->l_dst, l->l_ep,
						 l->l_length, l->l_cnt,
						 (interval) 0); 
#endif
			l->l_pkt = 0;
			l->l_next = l_free;	/* put on free list */
			l_free = l;
		    }
		    if(k == 0) l = l_list; /* next l */
		    else l = k->l_next;
		}
	    } else if (l->l_timeout < 0) {
		MON_EVENT("int_sweep: could not locate destination");
		dst = l->l_dst;
		pkt = l->l_pkt;
		l->l_pkt = 0;
		ident = l->l_intf->if_ident;
		notdeliver = l->l_intf->if_notdeliver;
		if(l->l_trusted && adr_count(&l->l_dst, (f_hopcnt_t *)0, NONTW,
					     UNSECURE) >= l->l_cnt) {
		    /* The destination exists but is not reachable through
		     * trusted paths.
		     */
		    f |= FLIP_UNTRUSTED;
		}
		if(k == 0) l_list = l->l_next; /* remove from list */
		else k->l_next = l->l_next;
		if(l->l_state == SLEEPING) {
		    l->l_state = FAILED;
		    wakeup((event) &l->l_locevent);
		} else {
		    l->l_next = l_free;	/* put on free list */
		    l_free = l;
		    if(notdeliver != 0) {
			(*notdeliver)(ident, pkt, &dst, 0, 0, 0, 0, f);
		    } else pkt_discard(pkt);
		}
		if(k == 0) l = l_list; /* next l */
		else l = k->l_next;
	    } else {
		sndlocate(l);
		k = l;
		l = l->l_next;
	    }
	}
    }
}


void
int_init(){
    alist_p al;
    locate_p loc;
    int i;
    intface_p intf;
    
    intface = (intface_p)
			aalloc((vir_bytes)(int_maxint * sizeof(intface_t)), 0);
    intfaceindex = (intface_p *)
			aalloc((vir_bytes)(int_maxint * sizeof(intface_p)), 0);
    for(intf = intface; intf < intface + int_maxint; intf++) {
	intf->if_state = IF_FREE;
	for(i=0; i < MAXEP; i++) intf->if_endpoint[i] = 0;
    }
    for(i = 0 ; i < (int) int_maxint; i++) intfaceindex[i] = intface+i;
    al_table = (alist_p) aalloc((vir_bytes)(NADDRESSES * sizeof(alist_t)), 0);
    for (al = al_table; al < &al_table[NADDRESSES]; al++) {
	al->al_next = al_free;
	al_free = al;
    }
    l_table = (locate_p) aalloc((vir_bytes)(NADDRESSES * sizeof(locate_t)), 0);
    for(loc = l_table; loc < l_table + NADDRESSES; loc++) {
	loc->l_next = l_free;
	l_free = loc;
    }
    l_list = 0;
    if_ntw = flip_newnetwork("interface", 0, INT_WEIGHT, 0, int_send,
			     int_broadcast, (void (*)()) 0, (void (*)()) 0, 0);
    sweeper_set(int_sweep, 100L, 100L, 0);
}
