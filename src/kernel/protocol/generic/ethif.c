/*	@(#)ethif.c	1.7	96/02/27 14:05:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Generic ethernet layer. Ethernet protocol numbers can be registered
 * using eth_proto_register.
 */

#include <string.h>
#include "amoeba.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "sys/flip/packet.h"
#include "sys/flip/eth_if.h"
#include "ethif.h"
#include "sys/flip/ethproto.h"
#include "sys/flip/ethpreamble.h"
#include "byteorder.h"
#include "machdep.h"
#include "sys/flip/flip.h"
#include "../flip/sys/routtab.h"
#include "sys/proto.h"
#include "sys/flip/measure.h"


#define MIN(a, b)	((a < b) ? a : b)
#define MAXETHERS	8
#define ETH_SIZE	((size_t) 6)

#ifndef SMALL_KERNEL
#ifdef STATISTICS
typedef struct ethstat {
    int eth_drop;
    int eth_sent;
    int eth_arrived;
} ethstat_t;

ethstat_t ethstat;

#define STINC(type)	ethstat.type++
    
eth_statistics(begin, end)
    char *begin;
    char *end;
{
    char *p;
    
    p = bprintf(begin, end, "========== ether statistics =======\n");
    p = bprintf(p, end, "dropped %7ld ", ethstat.eth_drop);
    p = bprintf(p, end, "sent    %7ld ", ethstat.eth_sent);
    p = bprintf(p, end, "arrived %7ld\n", ethstat.eth_arrived);
    
    return p - begin;
}

#else
#define STINC(type)
#endif
#else
#define STINC(type)
#endif


/*ARGSUSED*/
eth_discard(ifno, srcaddr, dstaddr, pkt)
    int ifno;
    char *srcaddr;
    char *dstaddr;
    struct packet *pkt;
{
    
    STINC(eth_drop);
    return 0;		/* meaning not interested anymore */
}


sei_t seilist[MAXETHERS];
sei_p seibound;
uint16 eth_maxeth;

extern hei_t heilist[];		/* board specific */

/*
 * Configuration of Ethernet interfaces
 */

#define MAXPROTO 	6

eps_t proto_switch[MAXPROTO+1];	/* One for eth_discard */
static int nproto;


void eth_register(proto, name, arrived)
    unsigned short proto;
    char *name;
    int (*arrived)();
{
    
    compare(nproto, <, MAXPROTO);
    enc_s_be(&proto);
    proto_switch[nproto].eps_proto = proto;
    proto_switch[nproto].eps_name = name;
    proto_switch[nproto].eps_arrived = arrived;
    nproto++;
    proto_switch[nproto].eps_arrived = eth_discard;
}


eth_praddr(eaddr)
    char *eaddr;
{
    
    printf("%x:%x:%x:%x:%x:%x",
	   eaddr[0]&0xFF,
	   eaddr[1]&0xFF,
	   eaddr[2]&0xFF,
	   eaddr[3]&0xFF,
	   eaddr[4]&0xFF,
	   eaddr[5]&0xFF
	   );
}

#ifdef DEBUG
static pr_ether_packet(p)
    char *p;
{
    register i;
    
    for(i=0;i<39;i++)
	printf("%2x",p[i]&0xFF);
    printf("\n");
}
#endif

static eth_prinfo(sp)
    register sei_p sp;
{
    
    printf("Ethernet interface %d: %s #%d has address ", 
	   sp - seilist, sp->sei_hei->hei_name, sp->sei_ifno);
    eth_praddr(sp->sei_ifaddr);
    printf("\n");
}

#ifndef NOMULTICAST
extern uint16 eth_nmcast;
ethmcast_p free_mcast;

eth_setmc(ifno, maddr)
    char *maddr;	/* the multicast address to be used */
{
    register sei_p sp;
    ethmcast_p em;
    
    sp = seilist+ifno;
    assert(sp<seibound);
    for(em = sp->sei_mcast; em != 0; em = em->em_next) {
	if(memcmp((_VOIDSTAR) maddr, (_VOIDSTAR) em->em_addr, ETH_SIZE) == 0) 
	    return(1);
    }
    if(sp->sei_hei->hei_setmc) {
	if((em = free_mcast) == 0) {
	    printf("eth_setmc: out mcast buffers\n");
	    return(0);
	}
	free_mcast = em->em_next;
	em->em_next = sp->sei_mcast;
	sp->sei_mcast = em;
	(void) memmove((_VOIDSTAR) em->em_addr, (_VOIDSTAR) maddr, ETH_SIZE);
	assert(sp->sei_hei->hei_setmc != 0);
	(*sp->sei_hei->hei_setmc)(sp->sei_ifno, sp->sei_mcast);
	return(1);
    }
    return(0);
}


eth_delmc(ifno, maddr)
    char *maddr;
{
    register sei_p sp;
    ethmcast_p em, p;
    
    sp = seilist+ifno;
    assert(sp<seibound);
    p = 0;
    for(em = sp->sei_mcast; em != 0; em = em->em_next) {
	if(memcmp((_VOIDSTAR) maddr, (_VOIDSTAR) em->em_addr, ETH_SIZE) == 0) {
	    if(p == 0) sp->sei_mcast = em->em_next;
	    else p->em_next = em->em_next;
	    em->em_next = free_mcast;
	    free_mcast = em;
	    if(sp->sei_mcast == 0) {
		assert(sp->sei_hei->hei_setmc != 0);
		(*sp->sei_hei->hei_setmc)(sp->sei_ifno, sp->sei_mcast);
	    }
	    return(1);
	} else p = em;
    }
    return(1);
}
#endif

void eth_init() {
    register hei_p hp;
    register sei_p sp;
    register ifno;
#ifndef NOMULTICAST
    ethmcast_p ep;
    
    DPRINTF(0, ("eth_init: aalloc %d ethmcast_t of size %d\n", eth_nmcast,
		sizeof(ethmcast_t)));
    free_mcast = (ethmcast_p)
		    aalloc((vir_bytes)(eth_nmcast * sizeof(ethmcast_t)), 0);
    if(free_mcast == 0) panic("eth_init: not enough memory");
    for(ep = free_mcast; ep < free_mcast + eth_nmcast - 1; ep++) {
	ep->em_next = ep + 1;
    }
    ep->em_next = 0;
#endif
    compare(sizeof(eh_t), ==, 14);
    
    proto_switch[0].eps_arrived = eth_discard;
    
    sp = seilist;
    for (hp=heilist; hp->hei_name; hp++) {
	(*hp->hei_alloc)(hp->hei_nif);
	for (ifno=0;ifno<hp->hei_nif;ifno++) {
	    if ((*hp->hei_init)(ifno, sp-seilist, sp->sei_ifaddr,
				&sp->sei_nrcvpkt,
				&sp->sei_nsndpkt)) {
		sp->sei_hei = hp;
		sp->sei_ifno = ifno;
		sp->sei_mcast = 0;
		eth_prinfo(sp);
		sp++;
		eth_maxeth++;
	    }
	}
    }
    assert(sp != seilist);
    seibound = sp;
}


void eth_info(ifno, ifaddr, nrcvpkt, nsndpkt)
    int ifno;
    char *ifaddr;
    int *nrcvpkt, *nsndpkt;
{
    register sei_p sp;
    
    sp = seilist+ifno;
    assert(sp<seibound);
    if(ifaddr != 0)
	(void) memmove((_VOIDSTAR) ifaddr, (_VOIDSTAR) sp->sei_ifaddr,
			ETH_SIZE);
    if(nrcvpkt != 0) *nrcvpkt = sp->sei_nrcvpkt;
    if(nsndpkt != 0) *nsndpkt = sp->sei_nsndpkt;
}


#define eth_set_adr(adr1, adr2) \
    * (short *) (adr1) = * (short *) (adr2); \
    * (short *) ((adr1) + 2) = * (short *) ((adr2) + 2); \
    * (short *) ((adr1) + 4) = * (short *) ((adr2) + 4);


void eth_send(ifno, pkt, dstaddr, proto)
    int ifno;			/* Which interface */
    struct packet *pkt;		/* The packet without linklevel header */
    char *dstaddr;		/* The ethernet address */
    unsigned short proto;	/* The protocol */
{
    register eh_p eh;
    register sei_p sp = seilist + ifno;
    static char nullbuf[60];
    int s;
   
    BEGIN_MEASURE(ether_snd);
    assert(sp<seibound);
    assert(pkt->p_contents.pc_totsize <= 1500);
    PROTO_ADD_HEADER(eh, pkt, eh_t);
    * (short *) eh->eh_dstaddr = * (short *) dstaddr;
    * (short *) (eh->eh_dstaddr+2) = * (short *) (dstaddr+2);
    * (short *) (eh->eh_dstaddr+4) = * (short *) (dstaddr+4);
    * (short *) eh->eh_srcaddr = * (short *) sp->sei_ifaddr;
    * (short *) (eh->eh_srcaddr+2) = * (short *) (sp->sei_ifaddr+2);
    * (short *) (eh->eh_srcaddr+4) = * (short *) (sp->sei_ifaddr+4);
    eh->eh_proto = proto;
    enc_s_be(&eh->eh_proto);
    proto_fix_header(pkt);
    if (pkt->p_contents.pc_totsize < 60) {
	/*
	 * We are required to send packets of 64 bytes and more
	 * including 4 byte CRC. So pad it to 60 without CRC.
	 */
	compare(pkt->p_contents.pc_dirsize, <=, pkt->p_contents.pc_totsize);
	if (pkt->p_contents.pc_dirsize != pkt->p_contents.pc_totsize)
	    pkt_pullup(pkt, pkt->p_contents.pc_totsize); 
	assert(pkt->p_contents.pc_totsize == pkt->p_contents.pc_dirsize);
	if (pkt->p_contents.pc_dirsize < 60) {
	    s = 60 - pkt->p_contents.pc_dirsize;
	    proto_dir_append(pkt, nullbuf, s);	/* macro! */
	}
    }
    STINC(eth_sent);
    assert(pkt->p_contents.pc_totsize >= 60);
    (*sp->sei_hei->hei_send)(sp->sei_ifno, pkt);
}

/*
 * A inward bound packet arrived on an interface. Handle it.
 * If the packet is sent to a multicast address, we should check
 * if this machine is listening to the multicast address. In the current 
 * implementation this is left to higher layers in the protocol stack.
 */

void eth_arrived(ifno, pkt)
    register struct packet *pkt;	/* The packet with linklevel header */
{
    register eh_p eh;
    register eps_p epsp;
    
#ifdef SPEED_HACK
    if(eth_fast_path(pkt))
	return;
#endif
    PROTO_LOOK_HEADER(eh, pkt, eh_t);
    if (eh == 0) {
	DPRINTF(0, ("eth_arrived: throw packet away\n"));
	pkt_discard(pkt);
	return;
    }

    assert(seilist+ifno<seibound);
    for (epsp=proto_switch; epsp->eps_proto; epsp++)
	if (eh->eh_proto == epsp->eps_proto)
	    break;
    STINC(eth_arrived);
    /*
     * Found it, or default entry in table
     */
    END_MEASURE(ether_rcv);
    if((*epsp->eps_arrived)(ifno, pkt)==0) {
	/* don't need it anymore, so give it back */
	pkt_discard(pkt);
    }
}


void eth_stopall()
{
    register sei_p sp;
    
    for (sp=seilist; sp<seibound; sp++)
	(*sp->sei_hei->hei_stop)(sp->sei_ifno);
}


#ifdef SPEED_HACK

#include "sys/flip/flip.h"
#include "sys/flip/fragment.h"
#include "/usr/proj/amwork/src/kernel/protocol/flip/sys/routtab.h"
#include "/usr/proj/amwork/src/kernel/protocol/flip/sys/interface-prv.h"

#define NADR_TAB	128
#define hash(adr)	((adr)->u_long[0] & (NADR_TAB - 1))
#define ETH_DATA	(1514 - sizeof(eh_t) - sizeof(fc_hdr_t) -\
			 sizeof(flip_hdr)) 


typedef struct adr_fast_info {
    adr_t 	a_adr;
    void 	*a_intf;
} adr_fast_info_t, *adr_fast_info_p;

static adr_fast_info_t adr_tab[NADR_TAB];

void eth_register_adr(adr, intf)
    adr_p adr;
    void *intf;
{	
    int i = hash(adr);
    
    assert(i >= 0);
    assert(i < NADR_TAB);
    
    if(ADR_NULL(&(adr_tab[i].a_adr))) {
	printf("eth_register_adr %d: ", i);
	badr_print(0, 0, adr);
	printf("\n");
	adr_tab[i].a_adr = *adr;
	adr_tab[i].a_intf = intf;
    } else {
	printf("eth_register_adr: hash clash\n");
    }
}

static adr_t nulladdr;

void eth_unregister_adr(adr)
    adr_p adr;
{
    int i = hash(adr);
    
    assert(i >= 0);
    assert(i < NADR_TAB);
    
    if(ADR_EQUAL(&(adr_tab[i].a_adr), adr)) {
	printf("eth_unregister_adr %d: ", i);
	badr_print(0, 0, adr);
	printf("\n");
	adr_tab[i].a_adr = nulladdr;
	adr_tab[i].a_intf = 0;
    } else {
	printf("eth_unregister_adr: hash clash\n");
    }
}


location ether_location;
int ether_fast_location;
f_hopcnt_t ether_fast_hcnt;
extern route_p route_cache;	/* cache for most recent used routes. */

int eth_fast_path(pkt)
    pkt_p pkt;
{
    register eh_p eh = (eh_p) pkt_offset(pkt);
    register flip_hdr *fh = (flip_hdr *) ((char *) eh + sizeof(eh_t) +
					  sizeof(fc_hdr_t));
    register route_p r = route_cache + 1;
    int i = hash(&fh->fh_dstaddr);
    
    /* BUG: check also endianess, network number, and secure bit. */
    if(ADR_EQUAL(&fh->fh_dstaddr, &adr_tab[i].a_adr) && eh->eh_proto == EP_FLIP
       && fh->fh_type == FL_T_UNIDATA && fh->fh_offset == 0 && fh->fh_total ==
       fh->fh_length && fh->fh_total < ETH_DATA && !(ADR_NULL(&fh->fh_srcaddr))
       && fh->fh_act_hop <= fh->fh_max_hop) { 

	(void) memmove((_VOIDSTAR) ether_location.l_bytes,
		       (_VOIDSTAR) eh->eh_srcaddr, ETH_SIZE);
	ether_fast_location = 1;
	ether_fast_hcnt = fh->fh_act_hop;
	if(r->r_sadr != 0 && ADR_EQUAL(&(r->r_sadr->ai_address), 
				       &fh->fh_srcaddr) &&
	   LOC_EQUAL(&ether_location, &r->r_sloc->li_location)) {   
	    if(fh->fh_act_hop < r->r_sloc->li_hopcnt)
		r->r_sloc->li_hopcnt = fh->fh_act_hop;
	    r->r_sloc->li_idle = 0;
	} else adr_install(&fh->fh_srcaddr, 1, &ether_location, fh->fh_act_hop,
			   REMOTE, !(fh->fh_flags & FL_F_UNSAFE));
	
	pkt->p_contents.pc_offset += sizeof(eh_t) + sizeof(fc_hdr_t) +
	    sizeof(flip_hdr);
	pkt->p_contents.pc_dirsize = pkt->p_contents.pc_totsize = fh->fh_length;
	int_fast(((intface_p) (adr_tab[i].a_intf)), fh, pkt);
	return(1);
    }
    ether_fast_location = 1;
    return(0);
}

int eth_flip_build(p, dst)
    char *p;
    char **dst;
{
    register eh_p eh = (eh_p) (p - sizeof(eh_t));
    register sei_p sp;
    
    sp = seilist+0;
    assert(sp<seibound);
    
    eh->eh_proto = EP_FLIP;
    *dst = eh->eh_dstaddr;
    eth_set_adr(eh->eh_srcaddr, sp->sei_ifaddr);
    return(sizeof(eh_t));
}
#endif
