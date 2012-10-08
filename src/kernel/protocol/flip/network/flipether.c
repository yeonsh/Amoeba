/*	@(#)flipether.c	1.8	96/02/27 14:02:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This file contains the flip ethernet dependent code. It interacts
 * with a fragmentation module to do fragmentation and flow control.
 * The flow control scheme is implemented using credits. We do
 * not do flow control for flip msgs that fit in one packet or 
 * multidata flip msgs (I don't know how to do this in an efficient and
 * clean way). The latter is clearly a BUG.
 */

#include <string.h>
#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "sys/flip/fragment.h"
#include "sys/flip/switch.h"
#include "../sys/routtab.h"
#include "sys/flip/ethproto.h"
#include "sys/flip/ethpreamble.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "byteorder.h"
#include "sys/flip/fl_byteorder.h"
#include "sys/flip/eth_if.h"
#include "machdep.h"
#include "sys/proto.h"
#include "sys/flip/measure.h"

/*Forward*/
static void gotmcack _ARGS(( pkt_p pkt, location *loc ));
static void gotmcnak _ARGS(( pkt_p pkt, location *loc ));
static void gotmcreq _ARGS(( int dev, pkt_p pkt, location *loc ));

#define ETH_DATA	(1500L - sizeof(flip_hdr) - sizeof(fc_hdr_t))
#define ETH_WEIGHT	3

#define MON_EVENT(str)  DPRINTF(0, ("%s\n", str));

extern uint16 eth_maxeth;

static unsigned char flipbrdcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static int *eth_ntw;


/* When a packet coming from location r_location is released, we might have
 * to send a credits to the location. If we do it immediately when the packet
 * arrives, new packets might arrive to fast to process. So, we send credits
 * when a packet is completely processed. To be able to send credits back,
 * the location and device on which the packet arrived is stored in the
 * structure release when the packet comes in. When the packet is discarded,
 * this information is used to send credits back.
 */

typedef struct release {
    long	r_oldarg;		/* stack arg to pkt_release */
    void	(*r_oldrelease)(); 	/* stack release func */
    pkt_p 	r_pkt;			/* keep ptr to pkt */
    location	r_location;		/* keep loc from which pkt arrived */
    int		r_dev;			/* keep dev on which pkt arrived */
    int		r_credit;		/* is credit scheme used for pkt? */
    struct release *r_next;
} release_t, *release_p;

static release_p free_release;	/* free list of release structures */


#ifndef SMALL_KERNEL
#ifdef STATISTICS
typedef struct {
	int ff_sreq;
	int ff_scredit;
	int ff_spiggy;
	int ff_smcreq;
	int ff_smcack;
	int ff_smcnak;
	int ff_sacredit;
	int ff_rreq;
	int ff_rcredit;
	int ff_rpiggy;
	int ff_rmcreq;
	int ff_rmcack;
	int ff_rmcnak;
	int ff_racredit;
} ffstat_t, *ffstat_p;

static ffstat_t ffstat;

#define FSTINC(type)	ffstat.type++

int ffstatistics(begin, end)
    char *begin;
    char *end;
{
    char *p;
    
    p = bprintf(begin, end, "============== ff statistics =======\n");
    p = bprintf(p, end, "scredit   :%7ld ", ffstat.ff_scredit);
    p = bprintf(p, end, "spiggy    :%7ld ", ffstat.ff_spiggy);
    p = bprintf(p, end, "srequest  :%7ld ", ffstat.ff_sreq);
    p = bprintf(p, end, "smcreq    :%7ld\n", ffstat.ff_smcreq);
    p = bprintf(p, end, "smcack    :%7ld ", ffstat.ff_smcack);
    p = bprintf(p, end, "smcnak    :%7ld ", ffstat.ff_smcnak);
    p = bprintf(p, end, "sabscredit:%7ld\n", ffstat.ff_sacredit);

    p = bprintf(p, end, "rcredit   :%7ld ", ffstat.ff_rcredit);
    p = bprintf(p, end, "rpiggy    :%7ld ", ffstat.ff_rpiggy);
    p = bprintf(p, end, "rrequest  :%7ld ", ffstat.ff_rreq);
    p = bprintf(p, end, "rmcreq    :%7ld\n", ffstat.ff_rmcreq);
    p = bprintf(p, end, "rmcack    :%7ld ", ffstat.ff_rmcack);
    p = bprintf(p, end, "rmcnak    :%7ld ", ffstat.ff_rmcnak);
    p = bprintf(p, end, "rabscredit:%7ld\n", ffstat.ff_racredit);

    return(p - begin);
}
#else
#define FSTINC(type)
#endif	/* STATISTICS */
#else
#define FSTINC(type)
#endif


/* The fragmentation module tells us that we are allowed to send 
 * a packet. 
 */
static void notify(dev)
    int dev;
{
    location l;
    pkt_p p;
    fc_cnt_t credit;
    int r;
    fc_hdr_p fc;
    
    while((r = ff_get(eth_ntw[dev], &l, &p, &credit)) > 0) {
	proto_fix_header(p);
	if(credit > 0) FSTINC(ff_spiggy);
	PROTO_ADD_HEADER(fc, p, fc_hdr_t);
	fc->fc_type = FC_DATA;
	fc->fc_cnt = credit;
	proto_fix_header(p);
	eth_send(dev, p, l.l_bytes, EP_FLIP);
	if(r == 1) return;
    };
}


/* The fragmentation module tells us that we should ask for credits. */
static void req_credit(dev, locp, pkt)
    int dev;
    location *locp;
    pkt_p pkt;
{
    FSTINC(ff_sreq);
    eth_send(dev, pkt, locp->l_bytes, EP_FLIP);
}


/* When the packet is discarded, check if we have to send credits back
 * to the location where the packet came from.
 */
static void send_credit(arg)
    long arg;
{
    release_p r = (release_p) arg;
    pkt_p p, ff_snd_credit();
    void (*tmp)();
    long oldarg;
    pkt_p pkt;

    assert(r);
    if(r->r_credit && ff_rcv_credit(eth_ntw[r->r_dev], &r->r_location, 0,
				    EV_RELEASE)) {
    	notify(r->r_dev);
    }
    if((p = ff_snd_credit(eth_ntw[r->r_dev], &r->r_location, 0))) {
	FSTINC(ff_scredit);
	eth_send(r->r_dev, p, r->r_location.l_bytes, EP_FLIP);
    }
    pkt = r->r_pkt;
    tmp = r->r_oldrelease;
    oldarg = r->r_oldarg;
    r->r_oldrelease = 0;
    r->r_pkt= 0;
    r->r_next = free_release;
    free_release =  r;
    pkt->p_admin.pa_release = tmp;
    pkt->p_admin.pa_arg = oldarg;
    if(tmp != 0) (*tmp)(oldarg);
}


/* Store information in the release structure, so that we can send credits
 * back when the packet is discarded. If there are no release structures,
 * we do nothing; the source will gives us some time to process all the packets
 * that are apparently floating around in the kernel.
 */
static void setreleaseinfo(pkt, dev, loc, credit)
    pkt_p pkt;
    int dev;
    location *loc;
    int credit;
{
    release_p r = free_release;

    if (r == 0) {
	printf("flipether: out of release structures\n");
	return;
    }
    free_release = r->r_next;
    r->r_next = 0;
    r->r_pkt = pkt;
    r->r_oldarg = pkt->p_admin.pa_arg;
    r->r_oldrelease = pkt->p_admin.pa_release;
    pkt->p_admin.pa_release = 0;
    r->r_dev = dev;
    r->r_location = *loc;
    r->r_credit = credit;
    pkt_set_release(pkt, send_credit, (long) r);
}


/* An ethernet packet intended for the FLIP protocol has arrived.  After
 * removing flow-control information, give it to the packet switch.
 */
int fleth_arrived(dev, pkt)
    int dev;
    register pkt_p pkt;
{
    static location gloc;
    register flip_hdr *fh;
    register fc_hdr_p fc;
    register location *locp;
    register eh_p eh;
    fc_cnt_t rcredit;
    pkt_p p, ff_snd_credit();

    BEGIN_MEASURE(flip_rcv);
    proto_cur_header(eh, pkt, eh_t);
    assert(eh);
    locp = &gloc;
    (void) memmove((_VOIDSTAR)locp->l_bytes, (_VOIDSTAR) eh->eh_srcaddr, 6);
    proto_remove_header(pkt);
    PROTO_LOOK_HEADER(fc, pkt, fc_hdr_t);
    if(fc == 0) {
	MON_EVENT("flip packet without control header");
	return(0);
    }
    switch(fc->fc_type) {
    case FC_REQCREDIT:	 /* request for new credits */
	FSTINC(ff_rreq);
	proto_remove_header(pkt);
	if((p = ff_snd_credit(eth_ntw[dev], locp, 1))) {
	    FSTINC(ff_sacredit);
	    eth_send(dev, p, locp->l_bytes, EP_FLIP);
	}
	return(0);
    case FC_CREDIT:	/* packet with only credits; no data */
	proto_remove_header(pkt);
	FSTINC(ff_rcredit);
	if(ff_rcv_credit(eth_ntw[dev], locp, fc->fc_cnt, EV_CREDIT))
	    notify(dev);
	return(0);
    case FC_ABSCREDIT:	 /* packet with new credits to use */
	proto_remove_header(pkt);
	FSTINC(ff_racredit);
	if(ff_rcv_credit(eth_ntw[dev], locp, fc->fc_cnt, EV_ABSCREDIT))
	    notify(dev);
	return(0);
#ifndef NOMULTICAST
    case FC_MCREQ:	/* request to listen to a multicast address */
	FSTINC(ff_rmcreq);
	proto_remove_header(pkt);
	gotmcreq(dev, pkt, locp);
	return 0;
    case FC_MCACK:	/* the other side listens to the multicast address */
	FSTINC(ff_rmcack);
	proto_remove_header(pkt);
	gotmcack(pkt, locp);
	return 0;
    case FC_MCNAK:	/* the other side refuses to listen to the mcast adr */
	FSTINC(ff_rmcnak);
	proto_remove_header(pkt);
	gotmcnak(pkt, locp);
	return 0;
#endif
    case FC_DATA:	/* packet with data and possible some credits */
	rcredit = fc->fc_cnt;
	if(rcredit > 0) FSTINC(ff_rpiggy);
	proto_remove_header(pkt);
	PROTO_LOOK_HEADER(fh, pkt, flip_hdr);
	if(fh == 0) return(0);
	fl_orderhdr(fh);		/* little endian? big endian? */
	if(fh->fh_length + sizeof(flip_hdr) > pkt->p_contents.pc_totsize) {
	    MON_EVENT("fleth_arrived: bad packet\n");
	    return 0;
	}
	/* Problem with Ethernet which needs a minimum packet size.
	 * pc_totsize and pc_dirsize may be too large.  Here we hack
	 * in the values that they should have.
	 */
	pkt->p_contents.pc_dirsize = pkt->p_contents.pc_totsize =
	    sizeof(flip_hdr) + fh->fh_length;

	if(fh->fh_total > ETH_DATA && fh->fh_type == FL_T_UNIDATA) {
	    setreleaseinfo(pkt, dev, locp, 1);
	    if(ff_rcv_credit(eth_ntw[dev], locp, rcredit, EV_ARRIVE)) { 
		notify(dev);
	    }
	}

	pktswitch(pkt, eth_ntw[dev], locp);
	return 1;
    default:
	MON_EVENT("illegal type");
	return 0;
    }
}


/* The packet switch wants to send a point-to-point message on the
 * specified network to the specified location.
 */
void fleth_send(dev, pkt, loc)
    register pkt_p pkt;
    location *loc;
{
    register fc_hdr_p fc;
    flip_hdr *fh;
    static location l;
    pkt_p p;
    fc_cnt_t credit = 0;
    int r;

    proto_cur_header(fh, pkt, flip_hdr);
    if(fh->fh_total > ETH_DATA) {
	/* these packet types may have to be fragmented */
	if(ff_store(eth_ntw[dev], pkt, loc, &credit)) {
	    /* Note that this will only happen if a gateway forwards an already
	     * fragmented message.
	     */
	    /*
	    proto_cur_header(fh, pkt, flip_hdr);
	    printf("fleth_send: offset = %d, mid = %d\n",
		fh->fh_offset, fh->fh_messid);
	    */
	    proto_fix_header(pkt);
	    if(credit > 0) {
		FSTINC(ff_spiggy);
	    }
	    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
	    fc->fc_type = FC_DATA;
	    fc->fc_cnt = credit;
	    proto_fix_header(pkt);
	    END_MEASURE(flip_snd);
	    eth_send(dev, pkt, loc->l_bytes, EP_FLIP);
	    return;
	}
	while((r = ff_get(eth_ntw[dev], &l, &p, &credit)) > 0) {
	    proto_fix_header(p);
	    if(credit > 0) {
		FSTINC(ff_spiggy);
	    }
	    PROTO_ADD_HEADER(fc, p, fc_hdr_t);
	    fc->fc_type = FC_DATA;
	    fc->fc_cnt = credit;
	    proto_fix_header(p);
	    END_MEASURE(flip_snd);
	    eth_send(dev, p, l.l_bytes, EP_FLIP);
	    if(r == 1) return;
	}
    } else {	/* one packet message */
	proto_fix_header(pkt);
	PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
	fc->fc_type = FC_DATA;
	fc->fc_cnt = 0;
	proto_fix_header(pkt);
	END_MEASURE(flip_snd);
	eth_send(dev, pkt, loc->l_bytes, EP_FLIP);
    }
}


/* The packet switch wants to broadcast a message on the specified network.
 */
void fleth_broadcast(dev, pkt)
    register pkt_p pkt;
{
    fc_hdr_p fc;

    proto_fix_header(pkt);
    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
    fc->fc_type = FC_DATA;
    fc->fc_cnt = 0;
    proto_fix_header(pkt);
    eth_send(dev, pkt, (char *) flipbrdcast, EP_FLIP);
}


#ifndef NOMULTICAST
static void setmc(adr, loc)
    adr_p adr;
    location *loc;
{
    assert(adr != (adr_p) 0);

    loc->l_bytes[0] = 0x55;
    loc->l_bytes[1] = 0x55;
    loc->l_bytes[2] = adr->a_abytes[2];
    loc->l_bytes[3] = adr->a_abytes[3];
    loc->l_bytes[4] = adr->a_abytes[4];
    loc->l_bytes[5] = adr->a_abytes[5];
}


void fleth_delmc(dev, multi, adr)
    int dev;
    location *multi;
    adr_p adr;
{      
    location loc;

    loc = *multi;
    if(LOC_NULL(&loc)) {
	setmc(adr, &loc);
    }
#ifdef DEBUG
    printf("fleth_delmc: delete ");
    bloc_print(0, 0, &loc); printf("\n");
#endif
    eth_delmc(dev, loc.l_bytes);
}


void fleth_setmc(dev, ni, adr)
    int dev;
    struct ntw_info *ni;
    adr_p adr;
{
    location *loc, *hloc;
    struct loc_info *li;
    pkt_p pkt;
    fc_hdr_p fc;
    adr_p hadr;
    
    loc = &ni->ni_multicast;
    if(LOC_NULL(loc)) {
	setmc(adr, loc);
    }
#ifdef DEBUG
    printf("fleth_setmc: ask hosts to listen to ");
    bloc_print(0, 0, loc); printf("\n");
#endif
    /* Also set the hardware multicast address of this machine, in case
     * it was removed as a side effect of multicast locate by this host.
     * See the remark in flip/sys/switch.c, function f_locate().
     */
    if (!eth_setmc(dev, loc->l_bytes)) {
	DPRINTF(0, ("fleth_setmc: problem setting own mc addr\n"));
    }
    for(li = ni->ni_loc; li != 0; li = li->li_next) {
	if(li->li_mcast) continue;
	if((pkt = flip_pkt_acquire()) == 0) {
	    DPRINTF(0, ("fleth_setmc: WARNING out of packets\n"));
	    break;
	}
	proto_init(pkt);
	PROTO_ADD_HEADER(hadr, pkt, adr_t);
	*hadr = *adr;
	proto_fix_header(pkt);
	PROTO_ADD_HEADER(hloc, pkt, location);
	*hloc = *loc;
	proto_fix_header(pkt);
	PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
	fc->fc_type = FC_MCREQ;
	proto_fix_header(pkt);
	FSTINC(ff_smcreq);
	eth_send(dev, pkt, li->li_location.l_bytes, EP_FLIP);
    }
}


static void gotmcreq(dev, pkt, loc)
    int dev;
    pkt_p pkt;
    location *loc;
{
    pkt_p new;
    fc_hdr_p fc;
    location *mloc;
    adr_t adr, *hadr;
    int r;
    
    PROTO_LOOK_HEADER(mloc, pkt, location);
    assert(mloc);
    if(mloc == 0) return;
    proto_remove_header(pkt);
    PROTO_LOOK_HEADER(hadr, pkt, adr_t);
    assert(hadr);
    if(hadr == 0) return;
    adr = *hadr;
    proto_remove_header(pkt);
#ifdef DEBUG
    printf("Got request from device %d to listen to mc: ", dev);
    bloc_print(0, 0, mloc); printf("\n");
#endif
    r = eth_setmc(dev, mloc->l_bytes);
    if((new = flip_pkt_acquire()) == 0)
	return;
    proto_init(new);
    PROTO_ADD_HEADER(hadr, new, adr_t);
    *hadr = adr;
    proto_fix_header(new);
    PROTO_ADD_HEADER(fc, new, fc_hdr_t);
    if(r) {
	FSTINC(ff_smcack);
	fc->fc_type = FC_MCACK;
    } else {
	FSTINC(ff_smcnak);
	fc->fc_type = FC_MCNAK;
    }
    proto_fix_header(new);
    eth_send(dev, new, loc->l_bytes, EP_FLIP);
}


static void gotmcack(pkt, loc)
    pkt_p pkt;
    location *loc;
{
    struct loc_info *li;
    struct ntw_info *ni;
    int nmcast, found;
    adr_t adr, *hadr;
    
    PROTO_LOOK_HEADER(hadr, pkt, adr_t);
    assert(hadr);
    if(hadr == 0) return;
    adr = *hadr;

    ni = adr_lookup(&adr, (uint16 *) 0, (f_hopcnt_t *) 0, UNSECURE);
    if(ni == 0 || ni == NI_BROADCAST) {
	DPRINTF(0, ("address does not exist\n"));
	return;
    }
    for( ; ni != 0; ni = ni->ni_next) {
	found = 0;
	nmcast = 0;
	for(li = ni->ni_loc; li != 0; li = li->li_next) {
	    if(LOC_EQUAL(&li->li_location, loc)) {
		li->li_mcast = LI_MCAST;
		found = 1;
	    }
	    if(li->li_mcast == LI_MCAST) nmcast += li->li_count;
	}
	if(found) {
	    ni->ni_nmcast = nmcast;
	    return;
	}
    }
}


static void gotmcnak(pkt, loc)
    pkt_p pkt;
    location *loc;
{
    struct loc_info *li;
    struct ntw_info *ni;
    int found;
    adr_t adr, *hadr;
    
    PROTO_LOOK_HEADER(hadr, pkt, adr_t);
    assert(hadr);
    if(hadr == 0) return;
    adr = *hadr;
    proto_remove_header(pkt);

    ni = adr_lookup(&adr, (uint16 *) 0, (f_hopcnt_t *) 0, UNSECURE);
    if(ni == 0 || ni == NI_BROADCAST) {
	DPRINTF(0, ("address does not exist\n"));
	return;
    }
    for( ; ni != 0; ni = ni->ni_next) {
	found = 0;
	for(li = ni->ni_loc; li != 0; li = li->li_next) {
	    if(LOC_EQUAL(&li->li_location, loc)) {
		li->li_mcast = LI_NOMCAST;
		found = 1;
	    }
	}
	if(found) {
	    return;
	}
    }
}
#else /* NOMULTICAST */
#define fleth_setmc	(void (*)())0
#define fleth_delmc	(void (*)())0
#endif /* NOMULTICAST */

#define NRELEASE	100	/* minimum */
extern uint16 flip_npkt;

void
fleth_init()
{
    register dev;
    release_p r;
    int nrcvpkt, nsndpkt;
    int nrelease;
    
    assert(sizeof(fc_hdr_t) == 2);
    /* kernel might be configured with more packet buffers */
    nrelease = (flip_npkt > NRELEASE) ? flip_npkt : NRELEASE;
    free_release = (release_p)
			aalloc((vir_bytes)(nrelease * sizeof(release_t)), 0);
    for (r = free_release; r < free_release + nrelease - 1; r++) {
	r->r_next = r + 1;
    }
    r->r_next = 0;
    eth_ntw = (int *) aalloc((vir_bytes)(eth_maxeth*sizeof(int)), 0);
    for (dev = 0; dev < (int) eth_maxeth; dev++) {
	eth_ntw[dev] = flip_newnetwork("Ethernet", dev, ETH_WEIGHT, 0,
				       fleth_send, fleth_broadcast, fleth_setmc,
				       fleth_delmc, 2);
	eth_info(dev, (char *) 0, &nrcvpkt, &nsndpkt);
	ff_newntw(eth_ntw[dev], dev, ETH_DATA, (uint16) nrcvpkt,
			(uint16) nsndpkt, req_credit, (void (*)()) 0); 
    }
    eth_register(EP_FLIP, "flip", fleth_arrived);
}
