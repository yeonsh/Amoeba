/*	@(#)flgrp_header.h	1.5	94/04/06 08:37:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FLGRP_HEADER_H__
#define	__FLGRP_HEADER_H__

#define BC_NTYPE        19

/* Broadcast messages types: */
#define BC_LOCATE       0
#define BC_HEREIS       1
#define BC_JOINREQ      2
#define BC_JOIN         3
#define BC_LEAVEREQ     4
#define BC_LEAVE        5
#define BC_BCASTREQ     6
#define BC_BCAST        7
#define BC_ACK		8
#define BC_RETRANS      9
#define BC_SYNC         10
#define BC_STATE        11
#define BC_REFORMREQ    12
#define BC_VOTE         13
#define BC_RESULT       14
#define BC_RESULTACK    15
#define BC_ALIVE        16
#define BC_ALIVEREQ     17
#define BC_DEAD		18

/* There are two types of BC_ALIVEREQ messages: */
#define A_PTP		1
#define A_BCAST		2

#define F_BIGENDIAN	0x1


typedef struct {
        uint8      b_type;         	/* type */
	uint8	   b_flags;		/* flags */
	uint16	   b_reserved;		/* ... */
        g_index_t  b_cpu;          	/* member identifier */
        g_incarno_t  b_incarnation;  	/* incarnation number */
        g_seqcnt_t     b_seqno;        	/* global sequence number */
        g_msgcnt_t     b_messid;       	/* message identifier */
        g_seqcnt_t     b_expect;       	/* seq number expected by application */
} bchdr_t, *bchdr_p;


#define bc_highpriority(t) \
        (t == BC_RETRANS || t == BC_STATE || t == BC_VOTE || t == BC_ALIVE ||\
					t == BC_ALIVEREQ || t == BC_ACK)

#define bc_request(t) (t == BC_JOINREQ || t == BC_BCASTREQ || t == BC_LEAVEREQ)

#ifndef SMALL_KERNEL
void  bc_printtype _ARGS(( int t ));
void bc_print _ARGS(( bchdr_p hdr ));
#endif

#endif /* __FLGRP_HEADER_H__ */
