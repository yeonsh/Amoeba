/*	@(#)smif.c	1.5	96/02/27 14:02:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
INIT_ASSERT
#include <stddef.h>
#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "smif.h"
#include "sys/proto.h"

/* FLIP shared memory interface. */

#define UNLIMITED	10000000L
#define MAXSM		8


#ifndef SMALL_KERNEL
#ifdef STATISTICS
typedef struct smstat {
        int sm_broadcast;
        int sm_ptp;
        int sm_arrived;
} smstat_t;

smstat_t smstat;

#define STINC(type)     smstat.type++
    
flsm_stat(begin, end)
char *begin;
char *end;
{
    char *p;
    
    p = bprintf(begin, end, "========== sm statistics =======\n");
    p = bprintf(p, end, "broadcast %7ld ", smstat.sm_broadcast);
    p = bprintf(p, end, "sent    %7ld ", smstat.sm_ptp);
    p = bprintf(p, end, "arrived %7ld\n", smstat.sm_arrived);
    
    return p - begin;
}

#else
#define STINC(type)
#endif
#endif


static smi_t smilist[MAXSM];
static smi_p smibound;
static int *sm_ntw;
uint16 sm_maxsm;

extern hmi_t hmilist[];


void flsm_send(ifno, pkt, dstaddr)
    int ifno;		/* Which interface */
    struct packet *pkt;	/* The packet without linklevel header */
    location *dstaddr;	/* The sm address */
{
    smi_p sp;
    
    sp = smilist+ifno;
    assert(sp<smibound);
    proto_fix_header(pkt);
    STINC(sm_ptp);
    assert(sp->smi_hmi->hmi_send);
    (*sp->smi_hmi->hmi_send)(sp->smi_ifno, pkt, dstaddr);
}


void flsm_broadcast(ifno, pkt)
    int ifno;		/* Which interface */
    struct packet *pkt;	/* The packet without linklevel header */
{
    smi_p sp;
    
    sp = smilist+ifno;
    assert(sp<smibound);
    proto_fix_header(pkt);
    STINC(sm_broadcast);
    assert(sp->smi_hmi->hmi_broadcast);
    (*sp->smi_hmi->hmi_broadcast)(sp->smi_ifno, pkt);
}


/* A pkt arrives from sm; we assume it is a FLIP packet.
 * Pktswitch expects that proto_look_header is called for the flip header. 
 */
void sm_arrived(ifno, pkt, src)
int ifno, src;
register struct packet *pkt;
{
    smi_p sp;
    static location loc;
    flip_hdr *fh;
    
    sp = smilist+ifno;
    assert(sp<smibound);
    * (int *) (loc.l_bytes) = src;
    fh = proto_look_header(pkt, flip_hdr); 
    STINC(sm_arrived);
    pktswitch(pkt, sm_ntw[ifno], &loc);
}


flsm_init()
{
    hmi_p hp;
    smi_p sp;
    int ifno;
    short delay, loss, on;
    
    sp = smilist;
    for(hp = hmilist; hp->hmi_name; hp++) {
	(*hp->hmi_alloc)(hp->hmi_nif);
	for (ifno = 0; ifno < hp->hmi_nif; ifno++) {
	    if ((*hp->hmi_init)(ifno, sp-smilist, 
				&sp->smi_ifaddr)) {
		sp->smi_hmi = hp;
		sp->smi_ifno = ifno;
		sp++;
		sm_maxsm++;
	    }
	    else panic("flsm_init: initialization failed");
	}
    }
    smibound = sp;
    sm_ntw = (int *) aalloc(sm_maxsm * sizeof(int), 0);
    for(ifno = 0; ifno < sm_maxsm; ifno++) {
	sm_ntw[ifno] = flip_newnetwork("Sm", ifno, 1, 0,
				    flsm_send, flsm_broadcast, NULL, NULL, 0);
	delay = 0;
	loss = 0;
	on = 0;			/* switch it off */
	flip_control(sm_ntw[ifno], &delay, &loss, &on, NULL);
    }
}
