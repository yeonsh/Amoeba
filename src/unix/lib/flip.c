/*	@(#)flip.c	1.3	96/02/27 11:55:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/ioccom.h>
#include "amoeba.h"
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "uxint.h"
#include "sys/flip/interface.h"
#include "assert.h"

/* This file contains stubs for a FLIP device. */

#define FLIPDEV		"/dev/flip"    
#define IPDEV		"/dev/flipip"
#define NINTFACE	16
#define NFLPACKETS	100
#define PKTBUFSIZ	2000
#define STACKSIZE	4000
    
static struct uintf {
	int 			f_used;
	int 			f_fd;
	int 			(*f_receive)();
	int 			(*f_notdeliver)();
	int 			f_ident;
} flip[NINTFACE];

static struct ip_args	ia;
static char sstack[STACKSIZE];
static int flip_interrupt;
static int rpcinit, rpcep;
static int ipep = -1;

int _flip_notpresent;		/* is there a flip device? */

#ifdef lint
/*VARARGS*/
use(){}
#else
    badassertion(file, line)
    char *file;
{
    printf("FLIP assertion failed in %s, line %d\n", file, line);
    exit(1);
}
#endif


void flip_random(adr)
    adr_p adr;
{
    * (int *) adr = rand() + getpid();
    adr->a_space = 0;
}


static void flip_trap(sig, code, scp)
int sig, code;
struct sigcontext *scp;
{
    flip_interrupt = 1;
    printf("I");
}


static int 
flip_doit(fd, cmd, fa)
    int fd;
    int cmd;
    struct flip_args *fa;
{
    if (ioctl(fd, cmd, fa) != 0) {
	return(-1);
    }
    return fa->fa_status;
}


ip_doit(fd, cmd, ia)
    int fd;
    int cmd;
    struct ip_args *ia;
{
    if (ioctl(fd, cmd, ia) != 0)
	return -1;
    return ia->fa_status;
}


static struct sigstack ss, oss;
static struct sigvec sv;

int flip_init(){
    extern pool_t flip_pool;
    static pkt_t flip_bufs[NFLPACKETS];
    static char flip_data[NFLPACKETS*PKTBUFSIZ];
    
    pkt_init(&flip_pool, PKTBUFSIZ, flip_bufs, NFLPACKETS, flip_data,
	     (void (*)()) 0, 0L);
    ss.ss_sp = sstack + STACKSIZE;
    ss.ss_onstack = 0;
    sv.sv_flags |= SV_ONSTACK;
    sv.sv_handler = flip_trap;
    sigstack(&ss, 0);
    signal(SIGEMT, flip_trap);
}


int flip_open(ident, receive, notdeliver)
    long ident;
    int (*receive)();
    int (*notdeliver)();
{
    DIR *opendir(), *dirp;
    struct direct *d;
    char name[64];
    int ep;
    static int init;
    struct flip_args fa;

    if(!init) {
	init = 1;
	flip_init();
    }
    if((dirp = opendir("/dev")) == 0) {
	return(-1);
    }
    while((d = readdir(dirp)) != 0) {
	if(strncmp(d->d_name, "flip", 4) == 0) {
	    sprintf(name, "/dev/%s", d->d_name);
	    ep = atoi(d->d_name+4);
	    if ((!flip[ep].f_used)  && (flip[ep].f_fd = open(name, 0)) >= 0) {
		flip[ep].f_used = 1;
		break;
	    }
	}
    }
    closedir(dirp);
    if(d == 0) return(-1);
    flip[ep].f_ident = ident;
    flip[ep].f_notdeliver = notdeliver;
    flip[ep].f_receive = receive;
    fa.fa_ident = (long) &flip[ep];
    if(flip_doit(flip[ep].f_fd, F_OPEN, &fa) < 0) {
	flip[ep].f_used = 0;
	return(-1);
    }
    if(fa.fa_status < 0) {
	flip[ep].f_used = 0;
	return(-1);
    }
    return(ep);
}


int flip_end(ep)
{
    assert(flip[ep].f_used);
    close(flip[ep].f_fd);
}


int flip_register(ep, adr)
    int ep;
    adr_p adr;
{
    register r;
    struct flip_args fa;
    
    assert(flip[ep].f_used);
    fa.fa_addr = *adr;
    r = flip_doit(flip[ep].f_fd, F_REGISTER, &fa);
    return(r);
}


int flip_unregister(ep, src)
    int ep;
    int src;
{
    register r;
    struct flip_args fa;
    
    assert(flip[ep].f_used);
    fa.fa_ep = src;
    r = flip_doit(flip[ep].f_fd, F_UNREGISTER, &fa);
    return(r);
}


int flip_unicast(ep, msg, flags, dst, src, length)
    int ep;
    pkt_p msg;
    int flags;
    adr_p dst;
    int src;
{
    struct packet_contents *pc = &msg->p_contents;
    struct flip_args fa;
    register r;
    
    assert(msg->p_admin.pa_allocated);
    assert(msg->p_admin.pa_header.ha_type == HAT_ABSENT);
    assert(flip[ep].f_used);
    fa.fa_dirsize = pc->pc_dirsize;
    fa.fa_totsize = pc->pc_totsize;
    fa.fa_buffer = &pc->pc_buffer[pc->pc_offset];
    fa.fa_virtual = pc->pc_virtual;
    fa.fa_flags = flags;
    fa.fa_addr = *dst;
    fa.fa_ep = src;
    fa.fa_count = length;
    r = flip_doit(flip[ep].f_fd, F_UNICAST, &fa);
    pkt_discard(msg);
    return r;
}


int flip_broadcast(ep, msg, src, length, hcnt)
    int ep;
    pkt_p msg;
    int src;
    long length;
    short hcnt;
{
    struct packet_contents *pc = &msg->p_contents;
    struct flip_args fa;
    register r;
    
    assert(msg->p_admin.pa_allocated);
    assert(msg->p_admin.pa_header.ha_type == HAT_ABSENT);
    assert(flip[ep].f_used);
    fa.fa_dirsize = pc->pc_dirsize;
    fa.fa_totsize = pc->pc_totsize;
    fa.fa_buffer = &pc->pc_buffer[pc->pc_offset];
    fa.fa_virtual = pc->pc_virtual;
    fa.fa_ep = src;
    fa.fa_count = length;
    fa.fa_hopcnt = hcnt;
    r = flip_doit(flip[ep].f_fd, F_BROADCAST, &fa);
    pkt_discard(msg);
    return r;
}


int flip_wait(ep)
    int ep;
{
    pkt_p pkt, pkt_acquire();
    struct flip_args fa;
    struct uintf *intf;
    register r;
    
    assert(flip[ep].f_used);
    if((pkt = pkt_acquire()) == 0) {
	printf("flip_wait: out of packets\n");
	return;
    }
    intf = flip + ep;
    proto_init(pkt);
    fa.fa_buffer = &pkt->p_contents.pc_buffer[pkt->p_contents.pc_offset];
    r = flip_doit(flip[ep].f_fd, F_RECEIVE, &fa);
    printf("flip_doit -> %d\n", r);
    pkt->p_contents.pc_dirsize = fa.fa_dirsize;
    pkt->p_contents.pc_totsize = fa.fa_totsize;
    if(fa.fa_type == T_NEWMSG) {
	if(intf->f_receive != 0) {
	    (*intf->f_receive)(intf->f_ident, pkt, &fa.fa_addr, 
		fa.fa_messid, fa.fa_offset, 
		fa.fa_length, fa.fa_total);
	    return;
	}
    } else if(fa.fa_type == T_NOTDELIVER) {
	if(intf->f_notdeliver != 0) {
	    (*intf->f_notdeliver)(intf->f_ident, pkt, &fa.fa_addr, 
		0, 0, 0, 0, FLIP_UNKNOWN); 
	    return;
	}
    }
    pkt_discard(pkt);
}


static int rpc_init()
{
    DIR *opendir(), *dirp;
    struct direct *d;
    char name[64];
    int ep;

    if((dirp = opendir("/dev")) == 0) {
	return(-1);
    }
    while((d = readdir(dirp)) != 0) {
	if(strncmp(d->d_name, "flip", 4) == 0) {
	    sprintf(name, "/dev/%s", d->d_name);
	    ep = atoi(d->d_name+4);
	    if ((!flip[ep].f_used)  && (flip[ep].f_fd = open(name, 0)) >= 0) {
		flip[ep].f_used = 1;
		break;
	    }
	}
    }
    closedir(dirp);
    if(d == 0) return(-1);
    rpcep = ep;
    rpcinit = 1;
    return(1);
}


long rpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
    header *hdr1, *hdr2;
    bufptr buf1, buf2;
    f_size_t cnt1, cnt2;
{
    struct flip_args fa;
    int r;

    if(!rpcinit) {	/* Did this process call rpc_init? */
	if(rpc_init() < 0) {
	    _flip_notpresent = 1;
	    return(-1);
	}
    }
    fa.fa_hdr1 = *hdr1;
    fa.fa_buf1 = buf1;
    fa.fa_cnt1 = cnt1;
    fa.fa_buf2 = buf2;
    fa.fa_cnt2 = cnt2;
    r = flip_doit(flip[rpcep].f_fd, F_TRANS, &fa);
    if(!fa.fa_type) {	/* is this not initialized child? */
	if(rpc_init() < 0) {
	    _flip_notpresent = 1;
	    return(-1);
	}
	r = flip_doit(flip[rpcep].f_fd, F_TRANS, &fa);
    }
    if(!ERR_STATUS(r))    {
	*hdr2 = fa.fa_hdr2;
    }
    return(r);
}


long rpc_getreq(hdr, buf, cnt)
    header *hdr;
    bufptr buf;
    f_size_t cnt;
{
    int r;
    struct flip_args fa;

    if(!rpcinit) {
	if(rpc_init() < 0) {
	    _flip_notpresent = 1;
	    return(-1);
	}
    }
    fa.fa_hdr1 = *hdr;
    fa.fa_buf1 = buf;
    fa.fa_cnt1 = cnt; 
    r = flip_doit(flip[rpcep].f_fd, F_GETREQ, &fa);
    if(!fa.fa_type) {	/* is this not initialized child? */
	if(rpc_init() < 0) {
	    _flip_notpresent = 1;
	    return(-1);
	}
	r = flip_doit(flip[rpcep].f_fd, F_GETREQ, &fa);
    }
    if(!ERR_STATUS(r))    {
	*hdr = fa.fa_hdr1;
    }
    return(r);
}


long rpc_putrep(hdr, buf, cnt)
    header *hdr;
    bufptr buf;
    f_size_t cnt;
{
    struct flip_args fa;
    int r;

    if(!rpc_init) {
	if(rpc_init() < 0) {
	    _flip_notpresent = 1;
	    return(-1);
	}
    }
    fa.fa_hdr1 = *hdr;
    fa.fa_buf1 = buf;
    fa.fa_cnt1 = cnt;
    r = flip_doit(flip[rpcep].f_fd, F_PUTREP, &fa);
    if(!fa.fa_type) {	/* is this not initialized child? */
	if(rpc_init() < 0) {
	    _flip_notpresent = 1;
	    return(-1);
	}
	r = flip_doit(flip[rpcep].f_fd, F_PUTREP, &fa);
    }
    return(r);
}


flip_stat(buf, size)
	char *buf;
	int size;
{
    int r;
    struct flip_args fa;

    if(!rpc_init) {
	if(rpc_init() < 0) {
	    printf("rpc_init failed\n");
	    return(-1);
	}
    }
    fa.fa_buffer = buf;
    r = flip_doit(flip[rpcep].f_fd, F_STATISTICS, &fa);
    return(r);
}


flip_dump(buf, size)
	char *buf;
	int size;
{
    struct flip_args fa;
    int r;

    if(!rpcinit) {
	if(rpc_init() < 0) {
	    printf("rpc_init failed\n");
	    return(-1);
	}
    }
    fa.fa_buffer = buf;
    fa.fa_cnt1 = size;
    r = flip_doit(flip[rpcep].f_fd, F_DUMP, &fa);
    return(r);
}
    

flip_control(ntw, delay, loss, on)
	short ntw;
	short *delay;
	short *loss;
	short *on;
{
    struct flip_args fa;
    int r;

    if(!rpcinit) {
	if(rpc_init() < 0) {
	    printf("rpc_init failed\n");
	    return(-1);
	}
    }
    fa.fa_dirsize = ntw;
    fa.fa_totsize = *on;
    fa.fa_virtual = *loss;
    fa.fa_count = *delay;
    r = flip_doit(flip[rpcep].f_fd, F_CONTROL, &fa);
    *on= fa.fa_totsize;
    *loss= fa.fa_virtual;
    *delay= fa.fa_count;
    return(r);
}
    

int 
flip_ipopen()
{
    
    ipep = open(IPDEV, 0);
    if(ipep < 0)  {
	perror("flip_ipopen failed");
	return(0);
    } else {
	return(1);
    }
}


int
flip_ipwrite(msg, loc)
        pkt_p msg;
        location *loc;
{
    struct packet_contents *pc = &msg->p_contents;
    register r;

    assert(msg->p_admin.pa_allocated);
    ia.fa_dirsize = pc->pc_dirsize;
    ia.fa_totsize = pc->pc_totsize;
    ia.fa_buffer = &pc->pc_buffer[pc->pc_offset];
    ia.fa_virtual = pc->pc_virtual;
    ia.fa_loc = *loc;
    r = ip_doit(ipep, F_IPWRITE, &ia);
    return(r);
}


int
flip_ipread(msg, loc)
    pkt_p *msg;
    location *loc;
{
    pkt_p pkt = *msg;
    register r;

    proto_init(pkt);
    ia.fa_buffer = pkt_offset(pkt);
    ia.fa_totsize = pkt->p_admin.pa_size - pkt->p_contents.pc_offset;
    r = ip_doit(ipep, F_IPREAD, &ia);
    if(r >= 0) {
            pkt->p_contents.pc_dirsize = ia.fa_dirsize;
            pkt->p_contents.pc_totsize = ia.fa_totsize;
	    assert(pkt->p_contents.pc_dirsize == pkt->p_contents.pc_totsize);
	    *loc = ia.fa_loc;
	    return(1);
    }
    return(0);
}
