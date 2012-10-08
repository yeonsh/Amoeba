/*	@(#)trans.H	1.3	94/04/06 17:19:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  WARNING:
**	This file is included by kthread.h directly into a struct definition.
**	As a consequence you cannot define anything except macros and
**	additions inside the trans struct.
*/

struct trans {
	char		TS_state;	/* see below */
	char		TS_flags;	/* several flags - see below */
	char		TS_clident;	/* ident number for client */
	char		TS_svident;	/* ident number for server */
	char		TS_ident;	/* transaction identifier */
	char		TS_seq;		/* fragment sequence number */
	char		TS_count;	/* max. number timer may expire */
	char		TS_signal;	/* signal being sent to the server */
	address		TS_addr;	/* network address of this thread */
	uint16		TS_timer;	/* timer, decremented every sweep */
	uint16		TS_cltim;	/* client crash timer */
	address		TS_client;	/* if serving: who's the client */
	address		TS_server;	/* if in trans: who's the server */
	port		TS_portcache;	/* this port was used the last time */
	header	       *TS_rhdr;	/* saved param in getreq or trans */
	header	       *TS_xhdr;	/* saved param in putrep or trans */
	phys_bytes	TS_rbaddr;	/* receiver physical address */
	phys_bytes	TS_xbaddr;	/* transmitter physical address */
	uint16		TS_rcnt;	/* size of rbuf */
	uint16		TS_xcnt;	/* size of xbuf */
	uint16		TS_offset;	/* offset in buffer */
	uint16		TS_maxloc;	/* max. location time in dsecs */
#ifdef USETICKER
	long		TS_totloc;	/* total location time in ticks */
	long		TS_totsvr;	/* total server time in ticks */
#endif
} tk_trans;

#ifdef TRANS

#define ts_state	tk_trans.TS_state
#define ts_flags	tk_trans.TS_flags
#define ts_clident	tk_trans.TS_clident
#define ts_svident	tk_trans.TS_svident
#define ts_ident	tk_trans.TS_ident
#define ts_seq		tk_trans.TS_seq
#define ts_timer	tk_trans.TS_timer
#define ts_count	tk_trans.TS_count
#define ts_signal	tk_trans.TS_signal
#define ts_addr		tk_trans.TS_addr
#define ts_cltim	tk_trans.TS_cltim
#define ts_client	tk_trans.TS_client
#define ts_server	tk_trans.TS_server
#define ts_portcache	tk_trans.TS_portcache
#define ts_rhdr		tk_trans.TS_rhdr
#define ts_xhdr		tk_trans.TS_xhdr
#define ts_rbaddr	tk_trans.TS_rbaddr
#define ts_xbaddr	tk_trans.TS_xbaddr
#define ts_rcnt		tk_trans.TS_rcnt
#define ts_xcnt		tk_trans.TS_xcnt
#define ts_offset	tk_trans.TS_offset
#define ts_maxloc	tk_trans.TS_maxloc
#define	ts_totloc	tk_trans.TS_totloc
#define ts_totsvr	tk_trans.TS_totsvr
#define ts_sender	tk_trans.TS_sender
#define ts_savehdr	tk_trans.TS_savehdr
#define ts_buffer	tk_trans.TS_buffer
#define ts_bufcnt	tk_trans.TS_bufcnt
#define ts_what		tk_trans.TS_what

/* possible values of ts_state */
#define IDLE		0
#define SENDING		1
#define DONE		2
#define ACKED		3
#define NACKED		4
#define FAILED		5
#define WAITBUF		6
#define RECEIVING	7
#define ABORT		8
#define MEMFAULT	9

/* possible flags in ts_flags */
#define LOCATING	0x0001	/* blocked in trans locating a port */
#define PUTREQ		0x0002	/* blocked in trans sending a request */
#define GETREQ		0x0004	/* blocked in getreq */
#define PUTREP		0x0008	/* blocked in putrep */
#define GETREP		0x0010	/* blocked in trans getting a reply */
#define SERVING		0x0020	/* running between getreq and putrep */
#define FASTKILLED	0x0040	/* (local) client is dead */

#else 

#define tk_trans	tk_dummy	/* other modules must not touch it */

#endif
