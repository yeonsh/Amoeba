/*	@(#)ux_rpc_int.h	1.6	96/02/27 11:55:11 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __UX_RPC_INT_H__
#define __UX_RPC_INT_H__

#include "module/rpc.h"
#include "ux_ctrl_int.h"	/* for STREAMS definition */

struct rpc_args {
#ifdef STREAMS
    int rpc_type;
#endif /* STREAMS */
    long rpc_status;
    int rpc_error;
    header rpc_hdr1;
    header rpc_hdr2;
    bufptr rpc_buf1;
    bufptr rpc_buf2;
    f_size_t rpc_cnt1;
    f_size_t rpc_cnt2;
};

#ifdef STREAMS
/* This is a terrible hack.  The streams-based version of the driver
 * can't find out the pid of the process that is accessing the device
 * so we send it down to the kernel in the rpc_status field.
 */
#define	rpc_pid		rpc_status
#endif /* STREAMS */

#ifdef __STDC__
#include "ioctlfix.h"

#define F_TRANS		_IOWR('f', FLRPC_TRANS, struct rpc_args)
#define F_GETREQ	_IOWR('f', FLRPC_GETREQ, struct rpc_args)
#define F_PUTREP	_IOWR('f', FLRPC_PUTREP, struct rpc_args)
#else
#define F_TRANS		_IOWR(f, FLRPC_TRANS, struct rpc_args)
#define F_GETREQ	_IOWR(f, FLRPC_GETREQ, struct rpc_args)
#define F_PUTREP	_IOWR(f, FLRPC_PUTREP, struct rpc_args)
#endif

#define MAXRPCBUF       1000

typedef struct msg {            /* Struct to buffer an incomming message. */
        struct msg *m_next;
        pkt_p m_pkt;
        long m_length;
        char *m_buf;
} msg_t, *msg_p;


#define MAXDIRECT       1000    /* Make sure that all the direct data fits
				 * in one network packet.  */

struct rpc_device {
        int rpc_open;           /* is it open? */

        int rpc_sndevent;       /* synchronization var fragmentation and rpc */

        struct proc *rpc_proc;  /* process that opened this device */
        int rpc_pid;            /* its pid for checking */
#ifdef STREAMS
	void *proc_ref;		/* process handle used for sending signals */
	void *mp; 		/* stream mblk used in putmsg(2)/getmsg(2) */
	void *mp_data; 		/* stream mblk with data to be sent */
	void *queue;
	f_hopcnt_t lochcnt;
	int loctime;
	int maxretrial;
#endif /* STREAMS */
	header rpc_rhdr;	/* incoming header */

        msg_p rpc_firstin;      /* queue with messages to be delivered */
        msg_p *rpc_lastin;
        int rpc_inpktcnt;

        pkt_p rpc_firstout;     /* queue with outcomming packets */
        pkt_p *rpc_lastout;
        int rpc_outcomplete;

        char *rpc_slpevent;     /* rpc buffering */
        int rpc_sleepflag;
        long rpc_timeout;

        char rpc_rpcdata[MAXRPCBUF]; /* buffer for local rpcs with data */
        int rpc_rpcdatacnt;     /* number of bytes buffered */
        int rpc_bufevent;       /*  synchronization var */
};

#endif /* __UX_RPC_INT_H__ */
