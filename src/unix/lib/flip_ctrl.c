/*	@(#)flip_ctrl.c	1.5	96/02/27 11:55:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/ioccom.h>
#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "ux_int.h"
#include "ux_ctrl_int.h"
#include "assert.h"

#ifdef STREAMS
#include <fcntl.h>
#include <stropts.h>
#endif

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"

static int ctrl_init, ctrl_fd;

static int ctrl_open()
{
    DIR *opendir(), *dirp;
    struct dirent *d;
    char name[64];

    if((dirp = opendir("/dev")) == 0) {
	return(-1);
    }
    while((d = readdir(dirp)) != 0) {
	if(strncmp(d->d_name, "flip_ctrl", 9) == 0) {
	    sprintf(name, "/dev/%s", d->d_name);
#ifdef STREAMS
	    if ((ctrl_fd = open(name, O_RDWR)) >= 0) {
#else
	    if ((ctrl_fd = open(name, 0)) >= 0) {
#endif
		break;
	    }
	}
    }
    closedir(dirp);
    if(d == 0) return(0);
    ctrl_init = 1;
    return(1);
}


#ifdef STREAMS

static errstat flip_doit(int fd, int cmd, struct ctrl_args *fa)
{
    struct strbuf ctrl_msg;	/* control part of stream message */
    struct strbuf data_msg;	/* data part of stream message */
    int flags = 0;
    int n;

    switch (cmd) {
	case F_STATISTICS:
	    fa->ctrl_type = FLCTRL_STAT;
	    break;
	case F_DUMP:
	    fa->ctrl_type = FLCTRL_DUMP;
	    break;
	case F_CONTROL:
	    fa->ctrl_type = FLCTRL_CTRL;
	    break;
	default:
	    fa->ctrl_status = STD_ARGBAD;
	    return fa->ctrl_status;
    }

    /* assemble and send a putmsg(2) to the driver */
    ctrl_msg.buf = (char *) fa;
    ctrl_msg.len = sizeof(struct ctrl_args);
    if (putmsg(fd, &ctrl_msg, 0, 0) != 0) {
	fa->ctrl_status = STD_SYSERR;
	return fa->ctrl_status;
    }

    /* assemble and send a getmsg(2) to the driver */
    ctrl_msg.buf = (char *) fa;
    ctrl_msg.maxlen = sizeof(struct ctrl_args);
    data_msg.buf = fa->ctrl_buffer;
    data_msg.maxlen = MAXPRINTBUF;
    n = getmsg(fd, &ctrl_msg, &data_msg, &flags);
    if (n < 0) {
	fa->ctrl_status = STD_SYSERR;
	return fa->ctrl_status;
    }

    if ((n == MORECTL) || (n == MOREDATA)) {
	/* this should not happen */
	fprintf(stderr,
		"file %s, line %d: getmsg returns MORECTL or MOREDATA\n",
		__FILE__, __LINE__);
	fa->ctrl_status = STD_SYSERR;
	return fa->ctrl_status;
    }

    return fa->ctrl_status;
}

#else  /* STREAMS */

static errstat flip_doit(fd, cmd, fa)
    int fd;
    int cmd;
    struct ctrl_args *fa;
{
    if (ioctl(fd, cmd, fa) != 0) {
	fa->ctrl_status = STD_SYSERR;
    }
    return fa->ctrl_status;
}

#endif  /* STREAMS */


errstat flctrl_stat(buf)
	char *buf;
{
    errstat r;
    struct ctrl_args fa;

    if(!ctrl_init) {
	if(!ctrl_open()) {
	    return(STD_SYSERR);
	}
    }
    fa.ctrl_buffer = buf;
    r = flip_doit(ctrl_fd, F_STATISTICS, &fa);
    return(r);
}


int flctrl_dump(buf)
	char *buf;
{
    struct ctrl_args fa;
    int r;

    if(!ctrl_init) {
	if(!ctrl_open() < 0) {
	    return(STD_SYSERR);
	}
    }
    fa.ctrl_buffer = buf;
    r = flip_doit(ctrl_fd, F_DUMP, &fa);
    return(r);
}
    

errstat flctrl_control(ntw, delay, loss, on, trusted)
	short ntw;
	short *delay;
	short *loss;
	short *on;
	short *trusted;
{
    struct ctrl_args fa;
    errstat r;

    if(!ctrl_init) {
	if(!ctrl_open()) {
	    return(STD_SYSERR);
	}
    }
    fa.ctrl_nw = ntw;
    fa.ctrl_on = *on;
    fa.ctrl_loss = *loss;
    fa.ctrl_delay = *delay;
    fa.ctrl_trusted = *trusted;
    r = flip_doit(ctrl_fd, F_CONTROL, &fa);
    *on= fa.ctrl_on;
    *loss= fa.ctrl_loss;
    *delay= fa.ctrl_delay;
    *trusted= fa.ctrl_trusted;
    return(r);
}
