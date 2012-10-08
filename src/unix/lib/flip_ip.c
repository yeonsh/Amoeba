/*	@(#)flip_ip.c	1.4	96/02/27 11:55:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/ioccom.h>
#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "ux_int.h"
#include "ux_ip_int.h"

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"

static int ip_init, ip_fd;
static struct ip_args	ia;

int flip_ip_open()
{
    DIR *opendir(), *dirp;
    struct dirent *d;
    char name[64];

    if((dirp = opendir("/dev")) == 0) {
	return(-1);
    }
    while((d = readdir(dirp)) != 0) {
	if(strncmp(d->d_name, "flip_ip", 7) == 0) {
	    sprintf(name, "/dev/%s", d->d_name);
	    if ((ip_fd = open(name, 0)) >= 0) {
		break;
	    }
	}
    }
    closedir(dirp);
    if(d == 0) return(0);
    ip_init = 1;
    return(1);
}

static errstat fl_ip_doit(fd, cmd, fa)
    int fd;
    int cmd;
    struct ip_args *fa;
{
    if (ioctl(fd, cmd, fa) != 0) {
	fa->ip_status = STD_SYSERR;
    }
    return fa->ip_status;
}


int
flip_ip_read(msg, loc)
	pkt_p *msg;
	location *loc;
{
    pkt_p pkt= *msg;
    register r;

    if(!ip_init) {
	return(STD_SYSERR);
    }
    proto_init(pkt);
    ia.ip_buffer = pkt_offset(pkt);
    ia.ip_totsize = pkt->p_admin.pa_size - pkt->p_contents.pc_offset;
    r = fl_ip_doit(ip_fd, F_IP_READ, &ia);
    if(r >= 0) {
	pkt->p_contents.pc_dirsize = ia.ip_dirsize;
	pkt->p_contents.pc_totsize = ia.ip_totsize;
	assert(pkt->p_contents.pc_dirsize == pkt->p_contents.pc_totsize);
	*loc = ia.ip_loc;
	return(1);
    }
    return(0);
}

int
flip_ip_write(msg, loc)
        pkt_p msg;
        location *loc;
{
    struct packet_contents *pc = &msg->p_contents;
    register r;

    if(!ip_init) {
	return(STD_SYSERR);
    }

    assert(msg->p_admin.pa_allocated);
    ia.ip_dirsize = pc->pc_dirsize;
    ia.ip_totsize = pc->pc_totsize;
    ia.ip_buffer = &pc->pc_buffer[pc->pc_offset];
    ia.ip_loc = *loc;
    r = fl_ip_doit(ip_fd, F_IP_WRITE, &ia);
    return(r);
}


