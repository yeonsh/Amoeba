/*	@(#)fragment.h	1.4	96/02/27 10:39:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define FC_DATA 	0x0
#define FC_REQCREDIT	0x1
#define FC_CREDIT	0x2
#define FC_MCREQ	0x3
#define FC_MCACK	0x4
#define FC_MCNAK	0x5
#define FC_ABSCREDIT	0x6
/* The next two fragment types are used in experimental protocols that
 * achieve a very low latency by doing everything on user level.
 */
#define FC_FAST_UNIDATA	  0x7
#define FC_FAST_MULTIDATA 0x8

typedef uint8  fc_type_t;
typedef uint8  fc_cnt_t;

typedef struct  {
	fc_type_t	fc_type;
	fc_cnt_t	fc_cnt;
} fc_hdr_t, *fc_hdr_p;

int ff_store _ARGS(( int ntwno, pkt_t *pkt, location *loc, fc_cnt_t *credit ));
int ff_get _ARGS(( int ntwno, location *loc, pkt_p *pkt, fc_cnt_t *credit ));
int ff_rcv_credit _ARGS(( int ntwno, location *loc, int /* fc_cnt_t */ cnt,
	int eventtype ));
pkt_t *ff_snd_credit _ARGS(( int ntwno, location *loc, int abs ));
#ifndef OLDFRAGMENTCODE
void ff_newntw _ARGS((
	int _ntw,
	int _devno, 
	f_size_t _mtu,
	int /* uint16 */ _nrcvpkt,
	int /* uint16 */ _nsndpkt,
	void (*_ff_request) _ARGS(( int _devno, location *_loc, pkt_p _pkt )),
	void (*_ff_cleanup) _ARGS(( location *_loc, pkt_p _pkt )) ));
#endif	/* OLDFRAGMENTCODE */
void ff_debug _ARGS(( void ));

/* Values of pkttype: */
#define	EV_ARRIVE	1	/* a data packet arrives */
#define EV_RELEASE	2	/* a data packet is released */
#define EV_CREDIT	3	/* a credit packet arrives */
#define EV_ABSCREDIT	4	/* an abs credit packet arrives */
