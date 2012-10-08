/*	@(#)flgrp_header.c	1.6	96/02/27 14:01:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef SMALL_KERNEL

#include "amoeba.h" 
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "group.h"
#include "flgrp_header.h"
#include "flgrp_hist.h"
#include "flgrp_type.h"
#include "flgrp_alloc.h"
#include "flgrp_hstpro.h"

/* This module implements routines for the broadcast-protocol header. */

void  bc_printtype(t)
    int t;
{
    switch(t) {
    case BC_LOCATE:
	printf("locate");
	break;
    case BC_HEREIS:
	printf("hereis");
	break;
    case BC_JOINREQ:
	printf("joinreq");
	break;
    case BC_JOIN:
	printf("join");
	break;
    case BC_LEAVEREQ:
	printf("leavereq");
	break;
    case BC_LEAVE:
	printf("leave");
	break;
    case BC_BCASTREQ:
	printf("bcastreq");
	break;
    case BC_BCAST:
	printf("bcast");
	break;
    case BC_ACK:
	printf("ack");
	break;
    case BC_RETRANS:
	printf("retrans");
	break;
    case BC_SYNC:
	printf("sync");
	break;
    case BC_STATE:
	printf("state");
	break;
    case BC_REFORMREQ:
	printf("reformreq");
	break;
    case BC_VOTE:
	printf("vote");
	break;
    case BC_RESULT:
	printf("result");
	break;
    case BC_RESULTACK:
	printf("resultack");
	break;
    case BC_ALIVE:
	printf("alive");
	break;
    case BC_ALIVEREQ:
	printf("alivereq");
	break;
    case BC_DEAD:
	printf("dead");
	break;
    default:
	printf("illegal bcast type\n");
    }
}


void bc_print(hdr)
    bchdr_p hdr;
{
    bc_printtype((int) hdr->b_type);
    printf(" s %d mid %d exp %d cpu %d incar %d", hdr->b_seqno, 
	   hdr->b_messid, hdr->b_expect, hdr->b_cpu, hdr->b_incarnation);
}
#endif /* SMALL_KERNEL */
