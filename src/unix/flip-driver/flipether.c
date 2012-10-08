/*	@(#)flipether.c	1.7	96/02/27 11:53:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "sys/flip/fragment.h"
#include "ux_int.h"
#include "ux_rpc_int.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <server/process/proc.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include "/usr/sys/net/if.h"
#include "/usr/sys/netinet/in.h"
#include "/usr/sys/netinet/if_ether.h"
#include "debug.h"
#include "assert.h"
INIT_ASSERT

#include "byteorder.h"
#include "sys/flip/fl_byteorder.h"

/* To get the prototypes right */
#include "machdep.h"
#include "sys/proto.h"

extern void flux_store();
extern void pktswitch();

/* This file contains the flip ethernet dependent code. It interacts
 * with a fragmentation module to do fragmentation and flow control.
 * The flow control scheme is implemented using credits. We do
 * not do flow control for flip msgs that fit in one packet or 
 * multidata flip msgs (i don't know how to this in an efficient and
 * clean way). The latter is clearly a BUG.
 */

#define MAXIFS		4
#define FLIP		0x8146
#define ETH_WEIGHT	3

#define ETH_DATA 	(1500L - sizeof(fc_hdr_t) - sizeof(flip_hdr))
#define ETH_EQUAL(a1, a2) ((* (short *) a1) == (* (short *) a2) &&\
    (* (short *) (a1+2)) == (* (short *) (a2 +2)) &&\
    (* (short *) (a1+4)) == (* (short *) (a2+4)))

static struct fleth {
	struct ifnet *fe_if;
	int fe_ntw;
} fleth[MAXIFS];
static nifs, busy;
static char flipbrdcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

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

#define STINC(type)	ffstat.type++

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
#define STINC(type)
#endif	/* STATISTICS */

/* When a packet coming from location r_location is released, we might have
 * to send a credits to the location. If we do it immediately when the packet
 * arrives, the packet might arrive to fast to process. So, we send credits
 * when a packet is completely processed. To be able to send credits back,
 * the location and device on which the packet arrived is stored in the
 * structure release when the packet comes in. When the packet is discarded,
 * this information is used to send credits back.
 */

typedef struct release {
    long	r_oldarg;
    void	(*r_oldrelease)();
    pkt_p 	r_pkt;
    location	r_location;
    int		r_dev;
    int		r_credit;
    struct release *r_next;
} release_t, *release_p;

static release_p free_release;	/* free list of release structures */

#ifndef MCLBYTES	/* SUN unix has MCLBYTES, others use CLBYTES */

#define MCLBYTES CLBYTES

/*
 * MCLGET turned from a macro into a routine.  So:
 */
struct mbuf *mclget(m)
    register struct mbuf *m;
{
    register struct mbuf *p;
    
#ifdef AMOEBA_BSD43
    MCLALLOC(p, 1);
#else
    MCLGET(m, p); /* At least on Ultrix 2.0; elsewhere maybe MCLGET(p,1) */
#endif AMOEBA_BSD43
    if (p == 0) {
	printf("mclget failed\n");
	m_freem(m);
	return p;
    }
    m->m_off = (int) p - (int) m;
    return m;
}
#endif MCLBYTES


static void notify(dev)
    int dev;
{
    void flsend();
    location l;
    pkt_p p;
    fc_cnt_t credit;
    int r;
    fc_hdr_p fc;
    int last;
    flip_hdr *fh;

    while((r = ff_get(fleth[dev].fe_ntw, &l, &p, &credit)) > 0) { 
	proto_cur_header(fh, p, flip_hdr);
	last = fh->fh_total == fh->fh_offset + fh->fh_length;
	proto_fix_header(p);
	PROTO_ADD_HEADER(fc, p, fc_hdr_t);
	fc->fc_type = FC_DATA;
	fc->fc_cnt = credit;
	proto_fix_header(p);
	if(credit > 0) STINC(ff_spiggy);
	if(p->p_contents.pc_dstype == FL_DS_USER) {
	    assert(p->p_contents.pc_virtual != 0);
	    proto_dir_append(p, &l, sizeof(location));
	    flux_store(p, last);
	} else {
	    flsend(dev, p, &l, FLIP);
	}
	if(r == 1) return;
    }
}

/* The fragmentation module tells us that we should ask for credits. */
static void req_credit(dev, locp, pkt)
    int dev;
    location *locp;
    pkt_p pkt;
{
    STINC(ff_sreq);
    flsend(dev, pkt, locp, FLIP);
}


/* The fragmentation layer thinks that locp is dead. */
/*ARGSUSED*/
static void failure(locp, pkt)
    location *locp;
    pkt_p pkt;
{
    void flux_failure();

    flux_failure(pkt);
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

    if(r->r_credit && ff_rcv_credit(fleth[r->r_dev].fe_ntw, &r->r_location, 0,
				    EV_RELEASE)) {
    	notify(r->r_dev);
    }
    if((p = ff_snd_credit(fleth[r->r_dev].fe_ntw, &r->r_location, 0))) {
	STINC(ff_scredit);
	flsend(r->r_dev, p, &r->r_location, FLIP);
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
 * back when the packet is discarded. If there are no release structure,
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

    if(r != 0) {
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
    } else {
	DPRINTF(0, ("setreleaseinfo: WARNING out of release structures\n"));
    }
}

static location loc;

/*ARGSUSED*/
static int flarrive(dev, dstaddr, srcaddr, pkt)
    int dev;
    char *dstaddr;
    char *srcaddr;
    register pkt_p pkt;
{
    
    /* Problem with Ethernet which needs a minimum packet size.
     * pc_totsize and pc_dirsize may be too large.  Here we hack
     * in the values that they should have.
     */
    register flip_hdr *fh;
    register fc_hdr_p fc;
    register location *locp;
    pkt_p p, ff_snd_credit();
    fc_cnt_t rcredit;
    void flsend();
    
    locp = &loc;
    bcopy(srcaddr, locp->l_bytes, 6);
    PROTO_LOOK_HEADER(fc, pkt, fc_hdr_t);
    if(fc == 0) {
	printf("flip packet without control header");
	return(0);
    }
    switch(fc->fc_type) {
    case FC_REQCREDIT:
	STINC(ff_rreq);
        proto_remove_header(pkt);
        if((p = ff_snd_credit(fleth[dev].fe_ntw, locp, 1))) {
	    STINC(ff_sacredit);
            flsend(dev, p, locp, FLIP);
        }
	return(0);
    case FC_CREDIT:
	STINC(ff_rcredit);
	proto_remove_header(pkt);
	if(ff_rcv_credit(fleth[dev].fe_ntw, locp, fc->fc_cnt, EV_CREDIT)) {
	    notify(dev);
	}
	return(0);
    case FC_ABSCREDIT:	 /* packet with new credits to use */
	STINC(ff_racredit);
	proto_remove_header(pkt);
	if(ff_rcv_credit(fleth[dev].fe_ntw, locp, fc->fc_cnt, EV_ABSCREDIT)) {
	    notify(dev);
	}
	return(0);
    case FC_MCREQ:		
	STINC(ff_rmcreq);
	return 0;
    case FC_MCACK:
	STINC(ff_rmcack);
	return 0;
    case FC_MCNAK:
	STINC(ff_rmcnak);
	return 0;
    case FC_DATA:
	rcredit = fc->fc_cnt;
	if(rcredit > 0) STINC(ff_rpiggy);
	proto_remove_header(pkt);
	PROTO_LOOK_HEADER(fh, pkt, flip_hdr);
	if(fh == 0) {
	    return(0);
	}
	fl_orderhdr(fh);
	if(fh->fh_length + sizeof(flip_hdr) > pkt->p_contents.pc_totsize) { 
	    printf("flip_arrived: bad packet\n");
	    return 0;
	}
	pkt->p_contents.pc_dirsize = pkt->p_contents.pc_totsize =
	    sizeof(flip_hdr) + fh->fh_length;
	if(fh->fh_total > ETH_DATA && fh->fh_type == FL_T_UNIDATA &&
	   ff_rcv_credit(fleth[dev].fe_ntw, locp, rcredit, EV_ARRIVE))
	{
	    notify(dev);
	}
	setreleaseinfo(pkt, dev, locp, fh->fh_total > ETH_DATA && fh->fh_type ==
		       FL_T_UNIDATA);
	pktswitch(pkt, fleth[dev].fe_ntw, locp);
	return 1;
    default:
	printf("illegal type");
	return 0;
    }
}

/*ARGSUSED*/
static struct ifqueue *fleth_arrived(ap, mb, eh)
    struct arpcom *ap;
    struct mbuf *mb;
    struct ether_header *eh;
{
    struct fleth *fe;
    struct mbuf *m;
    pkt_p pkt, flip_pkt_acquire();
    register x;
    
    if (busy) {		/* catch bouncing broadcast messages */
	m_freem(mb);
	return 0;
    }
    if (eh->ether_type != FLIP) {
	printf("FLIP: got packet of type %x\n", eh->ether_type);
	m_freem(mb);
	return 0;
    }
    for (fe = fleth; fe < &fleth[nifs]; fe++)
	if (fe->fe_if == * mtod(mb, struct ifnet **))
	    break;
    if (fe == &fleth[nifs]) {
	printf("FLIP: got packet from unknown interface\n");
	m_freem(mb);
	return 0;
    }
    mb->m_off += sizeof (struct ifnet *);
    mb->m_len -= sizeof (struct ifnet *);
    
    x = spl6();
    if ((pkt = flip_pkt_acquire()) == 0) {
	printf("fleth_arrived: out of packets\n");
	splx(x);
	m_freem(mb);
	return 0;
    }
    proto_setup_input(pkt, sizeof(fc_hdr_t));
    for (m = mb; m != 0; m = m->m_next)
	if (m->m_len != 0) {
	    assert(pkt->p_contents.pc_totsize + m->m_len <= pkt->p_contents.
		   pc_dirsize);
	    bcopy(mtod(m, char *), pkt_offset(pkt) +
		  pkt->p_contents.pc_totsize, m->m_len);
	    pkt->p_contents.pc_totsize += m->m_len;
	}
    m_freem(mb);
    if(!flarrive(fe-fleth, (char *) eh->ether_dhost.ether_addr_octet, 
	(char *) eh->ether_shost.ether_addr_octet, pkt))
	    pkt_discard(pkt);
    splx(x);
    return 0;
}


/* Flsend puts the message on the network. If the pkt contains virtual data,
 * flsend should be called the process that owns the virtual data.
 */
static void flsend(dev, pkt, loc, proto)
    int dev;
    register pkt_p pkt;
    location *loc;
    unsigned short proto;
{
    struct mbuf *m = 0, *chunk, *m_get();
    struct ifnet *ifp = fleth[dev].fe_if;
    struct sockaddr ea_addr;
    struct ether_header *eh = (struct ether_header *) ea_addr.sa_data;
    int size, csiz;
    int dirsize;
    int o = 0;
    int offset = 0;
    int r;
    
    size = pkt->p_contents.pc_totsize;
    dirsize = pkt->p_contents.pc_dirsize;
    assert(size <= ifp->if_mtu);
    busy = 1;
    while (size > 0) {
	chunk = m_get(M_DONTWAIT, MT_DATA);
	if (chunk == 0 || size > MLEN && !mclget(chunk)) {
	    printf("FLIP: out of mbufs\n");
	    if (m != 0)
		m_freem(m);
	    pkt_discard(pkt);
	    busy = 0;
	    return;
	}
	if (size > MLEN)
	    csiz = size <= MCLBYTES ? size : MCLBYTES;
	else
	    csiz = size;
	chunk->m_len = csiz;
	if(dirsize > 0) {
	    if(dirsize >= csiz) {
		bcopy(pkt_offset(pkt), mtod(chunk, char *), csiz);
		pkt->p_contents.pc_offset += csiz;
		dirsize -= csiz;
		size -= csiz;
		csiz = 0;
	    }
	    else {
		bcopy(pkt_offset(pkt), mtod(chunk, char *), dirsize);
		pkt->p_contents.pc_offset += dirsize;
		size -= dirsize;
		csiz -= dirsize;
		o += dirsize;
		dirsize = 0;
	    }
	}
	if(csiz > 0 && size > 0) {
	    if (pkt->p_contents.pc_dstype != FL_DS_USER)
	    {
		bcopy((char *) (pkt->p_contents.pc_virtual + offset),
			mtod(chunk, char *) + o, csiz);
	    }
	    else
	    {
		if((r=copyin((char *) (pkt->p_contents.pc_virtual + offset),
			     mtod(chunk, char *) + o, csiz)) != 0) {
		    printf("copy in failed: %d\n", r);
		    if (m != 0)
			m_freem(m);
		    pkt_discard(pkt);
		    busy = 0;
		    return;
		}
	    }
	    offset += csiz;
	    size -= csiz;
	    csiz = 0;
	    o = 0;
	}
	if (m == 0)
	    m = chunk;
	else
	    m_cat(m, chunk);
    }
    ea_addr.sa_family = AF_UNSPEC;
    bcopy(loc->l_bytes, (char *) &eh->ether_dhost, 6);
    eh->ether_type = proto;
    (*ifp->if_output)(ifp, m, &ea_addr);
    pkt_discard(pkt);
    busy = 0;
}


static void fleth_send(dev, pkt, loc)
    int dev;
    register pkt_p pkt;
    location *loc;
{
    register fc_hdr_p fc;
    flip_hdr *fh;
    static location l;
    pkt_p p, flux_getpkt();
    fc_cnt_t credit = 0;
    int last;
    int userdata = 0;
    
    proto_cur_header(fh, pkt, flip_hdr);
    if(fh->fh_total > ETH_DATA) {
	userdata = pkt->p_contents.pc_dstype == FL_DS_USER;
        if(ff_store(fleth[dev].fe_ntw, pkt, loc, &credit)) {
	    /* Note that this will only happen if a gateway forwards an already
	     * fragmented message.
	     */
	    assert(!(pkt->p_contents.pc_virtual != 0 && 
		pkt->p_contents.pc_dstype == FL_DS_USER));
	    if(credit > 0) STINC(ff_spiggy);
	    proto_fix_header(pkt);
	    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
	    fc->fc_type = FC_DATA;
	    fc->fc_cnt = credit;
	    proto_fix_header(pkt);
	    flsend(dev, pkt, loc, FLIP);
        }
	while(ff_get(fleth[dev].fe_ntw, &l, &p, &credit) > 0) {
	    if(p->p_contents.pc_dstype == FL_DS_USER) {
		/* packet with virtual data from user space. Store it,
		 * so that the owner of the data can send the packet.
		 */
		assert(p->p_contents.pc_virtual != 0);
		if(credit > 0) STINC(ff_spiggy);
		proto_cur_header(fh, p, flip_hdr);
		last = fh->fh_total == fh->fh_offset + fh->fh_length;
		proto_fix_header(p);
		PROTO_ADD_HEADER(fc, p, fc_hdr_t);
		fc->fc_type = FC_DATA;
		fc->fc_cnt = credit;
		proto_fix_header(p);
		proto_dir_append(p, &l, sizeof(location));
		flux_store(p, last);
	    } else {	/* packet with no user virtual data */
		proto_fix_header(p);
		if(credit > 0) STINC(ff_spiggy);
		PROTO_ADD_HEADER(fc, p, fc_hdr_t);
		fc->fc_type = FC_DATA;
		fc->fc_cnt = credit;
		proto_fix_header(p);
		flsend(dev, p, &l, FLIP);
	    }
	}
	if(userdata) {
	    /* Block until i am allowed to send. */
	    while((p = flux_getpkt()) != 0) {
		proto_dir_getoff(p, &l, sizeof(location));
		flsend(dev, p, &l, FLIP);
	    }
	}
    } else {	/* message fits in one Ethernet packet */
	proto_fix_header(pkt);
	PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
	fc->fc_type = FC_DATA;
	fc->fc_cnt = 0;
	proto_fix_header(pkt);
	flsend(dev, pkt, loc, FLIP);
    }
}


/* The packet switch wants to broadcast a message on the specified network.
 */
static void
fleth_broadcast(dev, pkt)
    int dev;
    pkt_p pkt;
{
    register fc_hdr_p fc;
    static location loc;

    proto_fix_header(pkt);
    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
    fc->fc_type = FC_DATA;
    fc->fc_cnt = 0;
    proto_fix_header(pkt);
    bcopy(flipbrdcast, loc.l_bytes, 6);
    flsend(dev, pkt, &loc, FLIP);
}


int fleth_stat(begin, end)
    char *begin, *end;
{
    char *p = begin;
    char *bprintf();
    struct ifnet *ifp;
    int i;

    for(i=0; i < nifs; i++) {
	ifp = fleth[i].fe_if;
	p = bprintf(p, end, "========= statistics %s ========\n", ifp->if_name);
	p = bprintf(p, end, "ip %d ie %d op %d oe %d col %d\n",
		    ifp->if_ipackets, ifp->if_ierrors, ifp->if_opackets,
		    ifp->if_oerrors, ifp->if_collisions);
    }
    p = bprintf(p, end, "========================\n");
    return(p - begin);
}

#define NRELEASE	100

fleth_init(){
    static struct ether_family flipfamily = {
	AF_UNSPEC, FLIP, fleth_arrived
	};
    struct ifnet *ifp;
    release_p r;
    
    assert(sizeof(fc_hdr_t) == 2);
    free_release =
	      (release_p) aalloc((vir_bytes) (NRELEASE * sizeof(release_t)), 0);
    for(r = free_release; r < free_release + NRELEASE - 1; r++) {
	r->r_next = r + 1;
    }
    r->r_next = 0;

    ether_register(&flipfamily);
    for (ifp = ifnet; ifp != 0; ifp = ifp->if_next)
	if ((ifp->if_flags & IFF_BROADCAST) && ifp->if_mtu == 1500) {
	    /* Ethernet */
	    if (nifs == MAXIFS) {
		printf("FLIP: too many interfaces\n");
		break;
	    }
	    else {
		fleth[nifs].fe_if = ifp;
		fleth[nifs].fe_ntw = flip_newnetwork("Ethernet", nifs,
					     ETH_WEIGHT, 0, fleth_send,
					     fleth_broadcast, (void (*)()) 0,
					     (void (*)()) 0, 2);
		ff_newntw(fleth[nifs].fe_ntw, nifs, ETH_DATA, 32, 45,
			  req_credit, failure);
		nifs++; 
	    }
	}
    if (nifs == 0)
	printf("FLIP: no network interfaces\n");
}
