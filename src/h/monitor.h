/*	@(#)monitor.h	1.4	96/02/27 10:26:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MONITOR_H__
#define __MONITOR_H__

/*
 * Include file for Amoeba server monitoring
 */

typedef struct {
	long	MN_cnt;		/* Number of times it happened */
	short	MN_soff;	/* Index in array of describing strings */
	short	MN_flags;	/* If nonzero action is needed */
} MN_ev_t, *MN_ev_p;

extern MN_ev_p _MN_init();
extern void    _MN_ainit();
extern void    _MN_occurred();
#define MON_EVENT(what)	do { static MN_ev_p MNmp;\
			  if (MNmp==0) MNmp=_MN_init(what);\
			  MNmp->MN_cnt++;\
			  if (MNmp->MN_flags) _MN_occurred(MNmp);\
			} while (0)
#define MON_SETBUF(buf,siz) _MN_ainit(buf,siz)

#define _getreq _MN_getreq
#define _putrep _MN_putrep
#define _trans _MN_trans

extern	bufsize	_trans _ARGS((header *, bufptr, bufsize,
			     header *, bufptr, bufsize));
extern	bufsize	_getreq _ARGS((header *, bufptr, bufsize));
extern	void	_putrep _ARGS((header *, bufptr, bufsize));

#define _rpc_getreq _MN_rpc_getreq
#define _rpc_putrep _MN_rpc_putrep
#define _rpc_trans _MN_rpc_trans

extern	bufsize	_rpc_trans _ARGS((header *, bufptr, bufsize,
			     header *, bufptr, bufsize));
extern	bufsize	_rpc_getreq _ARGS((header *, bufptr, bufsize));
extern	void	_rpc_putrep _ARGS((header *, bufptr, bufsize));

/*
 * From here on we have information hiding.
 * Normal monitored servers don't need to know this.
 */

#ifdef MONWHIZ		/* Only for monitoring routines */

#undef _getreq
#undef _putrep
#undef _trans

#undef _rpc_getreq
#undef _rpc_putrep
#undef _rpc_trans

#define DEFMONSIZE	2048
#define MAXMONSIZE	30000

typedef union {
	MN_ev_t	MN_mbuf[MAXMONSIZE/sizeof(MN_ev_t)];
	char	MN_cbuf[MAXMONSIZE];	/* array of describing strings */
	short	MN_sbuf[MAXMONSIZE/sizeof(short)];
} MN_b_t,*MN_b_p;
/*
 * defines for flags in MN_flags
 */

#define MNF_ITRACE 0x0001		/* Sent out trans on occurrence */
#define MNF_CIRBUF 0x0002		/* Keep circular buffer of events */
 
/*
 * redefines of fields in Amoeba header in monitor transactions
 */
 
#define MN_command	h_priv.prv_rights
#define MN_clport	h_priv.prv_random
#define MN_arg		h_extra
#define MN_size		h_size

/*
 * Different values for MN_command
 */

#define MNC_ENQUIRY	0		/* give current mondata */
#define MNC_SETAFLAGS	1		/* set all flags */
#define MNC_SETNFLAGS	2		/* set newevflags */
#define MNC_ITRACE	3		/* trace events using bitmap */
#define MNC_RESET	4		/* reset all counters */

/*
 * Values for h_status in answer
 */
 
#define MNS_UNKNOWN	((uint16) -1)	/* I beg your pardon?? */
#define MNS_OK		0		/* Understood and acted on */

/*
 * Values for h_command in transactions done from the monitored process
 */

#define MNT_ITRACE	1		/* An MNF_ITRACE flag hit */
#endif

#endif /* __MONITOR_H__ */
