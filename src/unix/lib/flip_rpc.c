/*	@(#)flip_rpc.c	1.5	96/02/27 11:55:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/ioccom.h>
#include "ux_int.h"
#include "ux_rpc_int.h"
#include "module/rpc.h"

#ifdef STREAMS
#include <fcntl.h>
#include <stropts.h>
#endif

#include "cmdreg.h"
#include "stderr.h"

/* This file contains stubs for a FLIP device. */

static int rpc_init, rpc_fd;

int _flip_notpresent;		/* is there a flip device? */


#ifdef STREAMS

static errstat flip_doit(int fd, int cmd, struct rpc_args *fa)
{
    struct strbuf ctrl_msg;	/* control part of stream message */
    struct strbuf data_msg;	/* data part of stream message */
    char *put_buf;
    int put_cnt;
    char *get_buf;
    int get_cnt;
    int flags = 0;
    int n;

    /* hdr1, buf1 and cnt1 are used for the putmsg(2), hdr2, buf2 and cnt2 */
    /* are used for the getmsg(2). Here we set them all to the right values. */
    switch (cmd) {
	case F_TRANS:
	    fa->rpc_type = FLRPC_TRANS;
	    put_buf = fa->rpc_buf1;
	    put_cnt = fa->rpc_cnt1;
	    get_buf = fa->rpc_buf2;
	    get_cnt = fa->rpc_cnt2;
	    break;
	case F_GETREQ:
	    fa->rpc_type = FLRPC_GETREQ;
	    fa->rpc_hdr2 = fa->rpc_hdr1;
	    fa->rpc_buf2 = fa->rpc_buf1;
	    fa->rpc_cnt2 = fa->rpc_cnt1;
	    put_buf = 0;
	    put_cnt = 0;
	    get_buf = fa->rpc_buf1;
	    get_cnt = fa->rpc_cnt1;
	    break;
	case F_PUTREP:
	    fa->rpc_type = FLRPC_PUTREP;
	    fa->rpc_hdr2 = fa->rpc_hdr1;
	    fa->rpc_buf2 = fa->rpc_buf1;
	    fa->rpc_cnt2 = fa->rpc_cnt1;
	    put_buf = fa->rpc_buf1;
	    put_cnt = fa->rpc_cnt1;
	    get_buf = 0;
	    get_cnt = 0;
	    break;
	default:
	    return(STD_ARGBAD);
    }

    /* We need to pass the PID, because the Solaris 2.x driver cannot find */
    /* the PID itself. */
    fa->rpc_pid = getpid();

    /* assemble and send a putmsg(2) to the driver */
    ctrl_msg.buf = (char *) fa;
    ctrl_msg.len = sizeof(struct rpc_args);
    data_msg.buf = put_buf;
    data_msg.len = put_cnt;
    if (putmsg(fd, &ctrl_msg, &data_msg, 0) != 0) {
	fa->rpc_status = STD_SYSERR;
	return fa->rpc_status;
    }

    /* assemble and send a getmsg(2) to the driver */
    ctrl_msg.buf = (char *) fa;
    ctrl_msg.maxlen = sizeof(struct rpc_args);
    data_msg.buf = get_buf;
    data_msg.maxlen = get_cnt;
    n = getmsg(fd, &ctrl_msg, &data_msg, &flags);
    if (n < 0) {
	if (ioctl(fd, I_FLUSH, FLUSHRW) == -1)
	    fprintf(stderr, "flip_doit: ioctl");
	fa->rpc_status = RPC_ABORTED;	/* STD_OK; */
	return fa->rpc_status;
    }

    if ((n == MORECTL) || (n == MOREDATA)) {
	/* this should not happen */
	fprintf(stderr,
		"file %s, line %d: getmsg returns MORECTL or MOREDATA\n",
		__FILE__, __LINE__);
    }

    return fa->rpc_status;
}

#else   /* STREAMS */

static errstat flip_doit(fd, cmd, fa)
int fd;
int cmd;
struct rpc_args *fa;
{
    if (ioctl(fd, cmd, fa) != 0) {
	fa->rpc_status = STD_SYSERR;
    }
    return fa->rpc_status;
}

#endif  /* STREAMS */


static int rpc_open()
{
    DIR *opendir(), *dirp;
    struct dirent *d;
    char name[64];

    if((dirp = opendir("/dev")) == 0) {
	return(-1);
    }
    while((d = readdir(dirp)) != 0) {
	if(strncmp(d->d_name, "flip_rpc", 8) == 0) {
	    sprintf(name, "/dev/%s", d->d_name);
#ifdef STREAMS
	    if ((rpc_fd = open(name, O_RDWR)) >= 0) {
#else
	    if ((rpc_fd = open(name, 0)) >= 0) { /* read-only? */
#endif /* STREAMS */
		break;
	    }
	}
    }
    closedir(dirp);
    if(d == 0) return(0);
    rpc_init = 1;
    return(1);
}


long flrpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
header *hdr1, *hdr2;
bufptr buf1, buf2;
bufsize cnt1, cnt2;
{
    struct rpc_args fa;
    long r;

    if(!rpc_init) {	/* Did this process call rpc_open? */
	if(rpc_open() <= 0) {
	    _flip_notpresent = 1;
	    return(STD_SYSERR);
	}
    }
    fa.rpc_hdr1 = *hdr1;
    fa.rpc_buf1 = buf1;
    fa.rpc_cnt1 = cnt1;
    fa.rpc_buf2 = buf2;
    fa.rpc_cnt2 = cnt2;
    r = flip_doit(rpc_fd, F_TRANS, &fa);
    if(!fa.rpc_error) {	/* is this not initialized child? */
	if(rpc_open() <= 0) {
	    _flip_notpresent = 1;
	    return(STD_SYSERR);
	}
	r = flip_doit(rpc_fd, F_TRANS, &fa);
    }
    if(!ERR_STATUS(r))    {
	*hdr2 = fa.rpc_hdr2;
    }
    return(r);
}


long rpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
header *hdr1, *hdr2;
bufptr buf1, buf2;
f_size_t cnt1, cnt2;
{
    return flrpc_trans(hdr1, buf1, (bufsize) cnt1, hdr2, buf2, (bufsize) cnt2);
}


long flrpc_getreq(hdr, buf, cnt)
header *hdr;
bufptr buf;
bufsize cnt;
{
    long r;
    struct rpc_args fa;

    if(!rpc_init) {
	if(rpc_open() <= 0) {
	    _flip_notpresent = 1;
	    return(STD_SYSERR);
	}
    }
    fa.rpc_hdr1 = *hdr;
    fa.rpc_buf1 = buf;
    fa.rpc_cnt1 = cnt; 
    r = flip_doit(rpc_fd, F_GETREQ, &fa);
    if(!fa.rpc_error) {	/* is this not initialized child? */
	if(rpc_open() <= 0) {
	    _flip_notpresent = 1;
	    return(STD_SYSERR);
	}
	r = flip_doit(rpc_fd, F_GETREQ, &fa);
    }
    if(!ERR_STATUS(r))    {
	*hdr = fa.rpc_hdr1;
    }
    return(r);
}


long rpc_getreq(hdr, buf, cnt)
header *hdr;
bufptr buf;
f_size_t cnt;
{
    return(flrpc_getreq(hdr, buf, (bufsize) cnt));
}


errstat flrpc_putrep(hdr, buf, cnt)
header *hdr;
bufptr buf;
bufsize cnt;
{
    struct rpc_args fa;
    errstat r;

    if(!rpc_init) {
	if(rpc_open() <= 0) {
	    _flip_notpresent = 1;
	    return(STD_SYSERR);
	}
    }
    fa.rpc_hdr1 = *hdr;
    fa.rpc_buf1 = buf;
    fa.rpc_cnt1 = cnt;
    r = flip_doit(rpc_fd, F_PUTREP, &fa);
    if(!fa.rpc_error) {	/* is this not initialized child? */
	if(rpc_open() <= 0) {
	    _flip_notpresent = 1;
	    return(STD_SYSERR);
	}
	r = flip_doit(rpc_fd, F_PUTREP, &fa);
    }
    return(r);
}


errstat rpc_putrep(hdr, buf, cnt)
header *hdr;
bufptr buf;
f_size_t cnt;
{
    return(flrpc_putrep(hdr, buf, (bufsize) cnt));
}
