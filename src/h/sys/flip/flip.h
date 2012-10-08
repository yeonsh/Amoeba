/*	@(#)flip.h	1.6	94/04/06 17:20:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SYS_FLIP_FLIP_H__
#define __SYS_FLIP_FLIP_H__

#include "protocols/flip.h"
#include "sys/flip/packet.h"

typedef struct flip_hdr {	/* FLIP header */
	f_vers_t	fh_version;		/* version */
	f_type_t	fh_type;		/* packet type */
	f_flag_t	fh_flags;		/* flags: see below */
	f_rsrv_t	fh_res;			/* reserved */	
	f_hopcnt_t	fh_act_hop;		/* actual hopcount */
        f_hopcnt_t	fh_max_hop;		/* maximal hopcount */
	adr_t		fh_dstaddr;		/* destination address */
	adr_t		fh_srcaddr;		/* source address */
	f_msgcnt_t	fh_messid;		/* message identifier */
	f_size_t	fh_length;		/* length of this packet */
	f_size_t	fh_offset;		/* offset within message */
	f_size_t	fh_total;		/* total length of message */
} flip_hdr;

#define FLIP_HDR_SIZE		40
#define FLIP_HEREIS_LENGTH	4

/*
 * fh_version
 */

#define FLIP_VERSION		1

/*
 * fh_type
 */

#define FL_T_LOCATE	1		/* locate packet */
#define FL_T_HEREIS	2		/* here-is packet */
#define FL_T_UNIDATA	3		/* unicast data packet */
#define FL_T_MULTIDATA	4		/* multicast data packet */
#define FL_T_NOTHERE	5		/* not-here packet */
#define FL_T_UNTRUSTED  6		/* untrusted packet */

/*
 * fh_flags
 */

/* input flags: */
#define FL_F_BIGENDIAN		0x01		/* Integers coded big endian */
#define FL_F_CODEFIELD		0x02		/* Code field present */
#define FL_F_SECURITY		0x04		/* Packet contents trusted */

/* output flags: */
#define FL_F_UNREACHABLE	0x10		/* can't route over trusted
						 * networks only. */
#define FL_F_UNSAFE		0x20		/* routed over untrusted
						 * network(s). */

/* prototypes */

struct ntw_info;

int flip_newnetwork _ARGS(( 
	char *_name,
	int _devno,
	int /* uint16 */ _weight,
	int /* int16 */ _loss,
	void (*_send) _ARGS(( int _dev, pkt_p _pkt, location *_loc )),
	void (*_bcast) _ARGS(( int _dev, pkt_p _pkt )), 
	void (*_setmc) _ARGS(( int _dev, struct ntw_info *_ni, adr_p _adr )),
	void (*_delmc) _ARGS(( int _dev, location *_multi, adr_p _adr )),
	int /* uint16 */ nloc ));

int flip_control _ARGS(( int network, short *delay, short *loss, short *on,
			short *trusted));

void pktswitch _ARGS(( pkt_p pkt, int ntw, location *loc ));

pkt_p flip_pkt_new _ARGS(( pkt_p pkt ));
pkt_p flip_pkt_acquire _ARGS(( void ));

/* Prototypes for flip internal functions that travel about in the flip code */

void rpc_myaddr _ARGS(( adr_p addr ));
void int_invalidate _ARGS(( adr_p adr ));

#endif /* __SYS_FLIP_FLIP_H__ */
