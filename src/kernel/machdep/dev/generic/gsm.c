/*	@(#)gsm.c	1.3	94/04/06 09:09:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include "assert.h"
INIT_ASSERT
#include "_ARGS.h"
#include "gsminfo.h"
#include "sys/flip/packet.h"

/* This module implements a message passing device through shared memory.
 * The implementation is much too general, but it allows each machine
 * to have multiple devices. Furthermore, it makes no assumptions about
 * which specific nodes it can reach through this device. Thus, one can
 * built arbitrary complex network configuration using these devices.
 */


/* The communication goes through buckets. In the most general case,
 * there is for each node an array of buckets. If node 10 wants to send some
 * thing to node 0 throught network 1, it looks in sm_info[1] where
 * slot 0's bucket array is. Then it looks in entry 10; if there is a packet
 * available, it fills it and set the flag FULL and generates an interrupt
 * at node 0. (To save some memory, each node allocates only buckets for nodes
 * 0 through the highest node on the network, given by networkspec.
 */

#define NBKT		20	/* maximum number of buckets per network */
#define MAXQUEUE	40	/* maximum number of packets in snd queue */

#define EMPTY	0
#define FULL	1
#define DONE	2

typedef struct sm_dev	*smd_p, smd_t;

    
typedef struct bucket {
    pkt_p	b_rcvpkt;	/* Incomming packet */
    pkt_p	b_sndpkt;	/* Sending packet */
    pkt_p	b_sndlast;	/* last packet in send queue */
    int		b_sndcnt;	/* # packets in send queue */
    smd_p	b_smp;		/* pointer to device struct */
    int		b_sndstate;	/* state for outgoing packet */
    int		b_rcvstate;	/* state for incoming packet */
} bk_t, *bk_p;


/* Each broadcast packet is stored in sm_bcast structure. If the
 * count reaches zero, then we discard bc_pkt. This makes sure that
 * virtual data is not reused by the sender of the bcast.
 */

#define NBCAST	 100

typedef struct sm_bcast {
    pkt_p	bc_pkt;
    int		bc_cnt;
} smbc_t, *smbc_p;


/* Nodes are numbered from 0 to some number. Some of these nodes may be
 * on the network; others not. For example, one can create a network
 * with nodes 4 and 5 on it and another network with nodes 8 and 10.
 * smd_networkspec gives the nodes on the network and nnode gives the number
 * of nodes on the network. Nbox gives the number of buckets that have
 * to be allocated.
 */

struct sm_dev {
    int		smd_up;			/* Up and running ? */
    int		smd_npkt;		/* # Packets in pool */
    int		smd_pktsize;		/* Size of a packet */
    pool_t	smd_pool;		/* Packet pool for incomming messages */
    pkt_p  	smd_pkt;		/* Packets for pool */
    char  	*smd_pktdata;		/* Data for packets */
    int		smd_ifaddr;		/* Address of this device */
    int		smd_softifno;		/* Identifier for higher layers */
    long 	(*smd_addrconv)();	/* address -> VME32-address */
    int		smd_nbox;		/* For each address an inbox */
    int 	smd_nnode;		/* number of nodes on the network */
    int		*smd_networkspec;	/* nodes on the network */
    bk_p  	smd_bk;			/* Incomming messages */
    int		smd_vec;		/* Interrupt vector */
    int		smd_intrarg;		/* Argument at interrupt */
};


static smd_p sm_data;
static int sm_ndevice;
static smbc_t outbcast[NBCAST];		/* outstanding bcasts */
extern sminfo_t sm_info[];


static void sm_release(ident)
    long ident;
{
    bk_p rcvbkp = (bk_p) ident;
    bk_p sndbkp;
    int src;
    smd_p smp;

    assert(rcvbkp);
    smp = rcvbkp->b_smp;
    assert(smp);
    src = rcvbkp - smp->smd_bk;


    PKT_GET(rcvbkp->b_rcvpkt, &smp->smd_pool);
    if(rcvbkp->b_rcvpkt == 0) {
	panic("sm_release out of packets");
    }
    proto_init(rcvbkp->b_rcvpkt);
    rcvbkp->b_rcvstate = EMPTY;

    sndbkp = (bk_p) (*smp->smd_addrconv)(src, smp->smd_bk + smp->smd_ifaddr);
    assert(sndbkp);
    sndbkp->b_sndstate = DONE;
    sendinterrupt(smp->smd_ifaddr, src);
}


static void sendnext(smp, dst)
    smd_p smp;
    int dst;
{
    register bk_p sndbk = smp->smd_bk + dst;
    register bk_p rcvbk;
    register pkt_p pkt1, pkt2;
    register char *p, *q;

    rcvbk = (bk_p) (*smp->smd_addrconv)(dst, smp->smd_bk + smp->smd_ifaddr);
    assert(rcvbk->b_rcvstate == EMPTY);
    assert(rcvbk->b_rcvpkt != 0);

    assert(sndbk->b_sndpkt);
    pkt1 = sndbk->b_sndpkt;
    pkt2 = (pkt_p) (*smp->smd_addrconv)(dst, rcvbk->b_rcvpkt);
    assert(pkt2);

    pkt2->p_admin.pa_header.ha_type = HAT_ABSENT;
    pkt2->p_contents.pc_dirsize = pkt1->p_contents.pc_dirsize;
    pkt2->p_contents.pc_offset = pkt1->p_contents.pc_offset;
    pkt2->p_contents.pc_totsize = pkt1->p_contents.pc_totsize;
    pkt2->p_contents.pc_dstype = pkt1->p_contents.pc_dstype;
    pkt2->p_contents.pc_dsident = pkt1->p_contents.pc_dsident;
    pkt2->p_contents.pc_virtual = pkt1->p_contents.pc_virtual;
    p = pkt1->p_contents.pc_buffer;
    assert(p);
    q = (char *) (*smp->smd_addrconv)(dst, pkt2->p_contents.pc_buffer);
    assert(q);
    assert(pkt2->p_admin.pa_size >= pkt1->p_admin.pa_size);
    (void) memmove((_VOIDSTAR)q, (_VOIDSTAR)p, (size_t) pkt1->p_admin.pa_size);

    sndbk->b_sndstate = FULL;
    rcvbk->b_rcvstate = FULL;
    sendinterrupt(smp->smd_ifaddr, dst);
}


/* Look for a bucket to which another node has copied message. */
static void sm_i_rintr(smp)
    smd_p smp;
{
    register bk_p bkp;
    register pkt_p pkt2;
    int src;
    int dst;
    pkt_p p1;
    int i;
    
    assert(smp->smd_up);
    for(i=0; i < smp->smd_nnode; i++) {
	bkp = smp->smd_bk + smp->smd_networkspec[i];
	switch(bkp->b_rcvstate) {
	case FULL:
	    bkp->b_rcvstate = DONE;
	    src = bkp - smp->smd_bk;
	    pkt2 = bkp->b_rcvpkt;
	    bkp->b_rcvpkt = 0;
	    assert(pkt2);
	    pkt2->p_contents.pc_virtual = (*smp->smd_addrconv)(src, 
						  pkt2->p_contents.pc_virtual);
	    pkt_set_release(pkt2, sm_release, (long) bkp);
	    sm_arrived(smp->smd_softifno, pkt2, src);  
	    break;
	}
    }
    for(i=0; i < smp->smd_nnode; i++) {
	bkp = smp->smd_bk + smp->smd_networkspec[i];
	switch(bkp->b_sndstate) {
	case DONE:
	    assert(bkp->b_sndpkt);
	    p1 = bkp->b_sndpkt;
	    bkp->b_sndpkt = p1->p_admin.pa_next;
	    p1->p_admin.pa_next = 0;
	    bkp->b_sndcnt--;
	    pkt_discard(p1);
	    if(bkp->b_sndpkt == 0) {
		assert(bkp->b_sndcnt == 0);
		bkp->b_sndstate = EMPTY;
		bkp->b_sndlast = 0;
	    } else {
		sendnext(smp, bkp - smp->smd_bk);
	    }
	    break;
	}
    }
}


/* Walk thought all devices and see on which device we got an interrupt. */
static void sm_intr(arg)
    int arg;	
{
    smd_p smp;
    
    for(smp = sm_data; smp < sm_data + sm_ndevice; smp++)
	if(smp->smd_intrarg == arg) {
	    disablemail(smp->smd_vec);
	    enqueue(sm_i_rintr, smp);
	}
}


#define DELAY 	10000


/* Send a message to node dst. */
static void dosend(smp, pkt1, dst)
    smd_p smp;
    pkt_p pkt1;
    int dst;
{
    register bk_p sndbk;

    assert(smp);
    assert(dst >= 0);
    compare(dst, <, smp->smd_nbox);
    assert(smp->smd_addrconv);	
    assert(pkt1);
    assert(pkt1->p_admin.pa_next == 0);

    sndbk = smp->smd_bk + dst;
    if(sndbk->b_sndstate != EMPTY) {
	assert(sndbk->b_sndpkt != 0);
	assert(sndbk->b_sndlast != 0);
	assert(sndbk->b_sndcnt > 0);
	if(sndbk->b_sndcnt > MAXQUEUE) {
	    printf("dosend: send overflow for %d\n", dst);
	    pkt_discard(pkt1);
	    return;
	}
	sndbk->b_sndcnt++;
	sndbk->b_sndlast->p_admin.pa_next = pkt1;
	sndbk->b_sndlast = pkt1;
	return;
    }
    assert(sndbk->b_sndpkt == 0);
    assert(sndbk->b_sndcnt == 0);
    assert(sndbk->b_sndlast == 0);
    sndbk->b_sndlast = pkt1;
    sndbk->b_sndcnt++;
    sndbk->b_sndpkt = pkt1;
    sendnext(smp, dst);
}


int sm_send(ifno, pkt1, dst)
    int ifno;
    pkt_p pkt1;
    int *dst;
{
    smd_p smp;

    assert(ifno < sm_ndevice);
    smp = sm_data + ifno;
    assert(smp->smd_up);
    assert(*dst != smp->smd_ifaddr);
    dosend(smp, pkt1, *dst);
}


static void bcast_discard(bcast)
    long bcast;
{
    pkt_p pkt;

    outbcast[bcast].bc_cnt--;
    if(outbcast[bcast].bc_cnt <= 0) {
	pkt = outbcast[bcast].bc_pkt;
	outbcast[bcast].bc_pkt = 0;
	if(pkt != 0) pkt_discard(pkt);
    }
}


int sm_broadcast(ifno, pkt1)
    int ifno;
    pkt_p pkt1;
{
    int i;
    smd_p smp;
    pkt_p pkt2;
    char *p, *q;
    int bcast;
    int send = 0;

    assert(ifno < sm_ndevice);
    smp = sm_data + ifno;
    assert(smp->smd_up);
    assert(pkt1);

    for(bcast = 0; bcast < NBCAST; bcast++) {
	if(outbcast[bcast].bc_pkt == 0) break; /* free? */
    }
    if(bcast >= NBCAST) {
	printf("sm_broadcast: out of bcasts; failed\n");
	pkt_discard(pkt1);
	return;
    }
    outbcast[bcast].bc_cnt = smp->smd_nnode - 1;
    outbcast[bcast].bc_pkt = pkt1;

    for(i=0; i < smp->smd_nnode; i++) {
	if(smp->smd_networkspec[i] != smp->smd_ifaddr) {
	    PKT_GET(pkt2, &smp->smd_pool);
	    if(pkt2 != 0) {
		pkt2->p_admin.pa_header.ha_type = HAT_ABSENT;
		pkt2->p_contents.pc_dirsize = pkt1->p_contents.pc_dirsize;
		pkt2->p_contents.pc_offset = pkt1->p_contents.pc_offset;
		pkt2->p_contents.pc_totsize = pkt1->p_contents.pc_totsize;
		pkt2->p_contents.pc_dstype = pkt1->p_contents.pc_dstype;
		pkt2->p_contents.pc_dsident = pkt1->p_contents.pc_dsident;
		pkt2->p_contents.pc_virtual = pkt1->p_contents.pc_virtual;
		p = pkt1->p_contents.pc_buffer;
		assert(p);
		q = pkt2->p_contents.pc_buffer;
		assert(q);
		assert(pkt2->p_admin.pa_size >= pkt1->p_admin.pa_size);
		(void) memmove((_VOIDSTAR) q, (_VOIDSTAR) p,
			       (size_t) pkt1->p_admin.pa_size);
		pkt_set_release(pkt2, bcast_discard, bcast);
		send = 1;
		dosend(smp, pkt2, smp->smd_networkspec[i]);
	    } else outbcast[bcast].bc_cnt--;
	}
    }
    if(!send) {
	pkt_discard(pkt1);
	outbcast[bcast].bc_pkt = 0;
	assert(outbcast[bcast].bc_cnt == 0);
    }
}


int sm_alloc(nif)
    int nif;
{
    printf("sm_alloc: alloc %d sm devices\n", nif);
    sm_data = (smd_p) aalloc(nif * sizeof(smd_t), 0);
    sm_ndevice = nif;
}


int sm_init(hardifno, softifno, ifaddr)
    int hardifno, softifno;
    int *ifaddr;
{
    sminfo_p info;
    smd_p smp;
    bk_p bk;
    int i;
    
    assert(hardifno < sm_ndevice);
    info = sm_info + hardifno;
    smp = sm_data + hardifno;
    smp->smd_bk = (bk_p) info->smi_devaddr;
    printf("sm_init: device from %X to %X size %X; vec %d\n", smp->smd_bk,
	   smp->smd_bk + NBKT, NBKT * sizeof(bk_t), info->smi_vec);
    if((*info->smi_getaddr)(info->smi_getarg, ifaddr) == 0)
	return(0);
    printf("sm_init: sm-interface has address: %d\n", *ifaddr);
    smp->smd_ifaddr = *ifaddr;
    smp->smd_softifno = softifno;
    smp->smd_npkt = info->smi_npkt;
    smp->smd_pktsize = info->smi_pktsize;
    smp->smd_addrconv = info->smi_addrconv;
    smp->smd_nbox = info->smi_maxaddr + 1;
    assert(smp->smd_nbox < NBKT);
    smp->smd_nnode = info->smi_nnode;
    smp->smd_networkspec = info->smi_networkspec;
    printf("sm_init: network: max addr %d with #nodes %d {", smp->smd_nbox,
	   smp->smd_nnode);
    for(i=0; i < smp->smd_nnode; i++) {
	printf("%d ", smp->smd_networkspec[i]);
    }
    printf("}\n");
    printf("sm_init: allocate %d pkts of size %d\n", smp->smd_npkt,
	   smp->smd_pktsize);
    smp->smd_pkt = (pkt_p) (*info->smi_memalloc)(sizeof(pkt_t) * 
						 smp->smd_npkt);
    smp->smd_pktdata = (*info->smi_memalloc)(smp->smd_npkt *
					     smp->smd_pktsize);
    assert(smp->smd_pkt != 0 && smp->smd_pktdata != 0);
    pkt_init(&smp->smd_pool, smp->smd_pktsize, smp->smd_pkt, smp->smd_npkt, 
	     smp->smd_pktdata, 0, 0);
    
    /* initializes buckets */
    for(i = 0; i < smp->smd_nbox; i++) {
	smp->smd_bk[i].b_rcvstate = EMPTY;
	smp->smd_bk[i].b_sndstate = EMPTY;
	smp->smd_bk[i].b_sndpkt = 0;
	smp->smd_bk[i].b_sndlast = 0;
	smp->smd_bk[i].b_sndcnt = 0;
	smp->smd_bk[i].b_smp = smp;
    }

    /* Allocate for each node on the network a packet, so that these nodes
     * can send a message to us.
     */
    for(i=0; i < smp->smd_nnode; i++) {
	bk = smp->smd_bk + smp->smd_networkspec[i];
	PKT_GET(bk->b_rcvpkt, &smp->smd_pool);
	if(bk->b_rcvpkt == 0) return(0);
    }

    /* Set the interrupt vector */
    smp->smd_vec = info->smi_vec;
    smp->smd_intrarg = setvec(smp->smd_vec, sm_intr);
    smp->smd_up = 1;
    return(1);
}
