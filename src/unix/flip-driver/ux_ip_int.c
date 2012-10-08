/*	@(#)ux_ip_int.c	1.3	94/04/07 14:03:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/user.h>
#include <assert.h>
INIT_ASSERT
#include <sys/user.h>
#include <sys/kernel.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "sys/debug.h"

#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "sys/flip/interface.h"

#include "ux_int.h"
#include "ux_rpc_int.h"
#include "ux_ip_int.h"

#define MAXNPKT		10	/* maximum number of queued incoming pkts. */
#define NIPPKT          50 	/* size of ip packet pool */
#define IPPKTSIZE       (PKTENDHDR+128)    /* the flip header and some data */
#define IPVIRSIZE       9000    /* 8K plus a bit */
    
typedef struct ip_msg {            /* Struct to buffer an incoming message. */
    struct ip_msg 	*m_next;
    pkt_p 		m_pkt;
    adr_t 		m_src;
    location 		m_loc;
    long 		m_messid;
    long 		m_offset;
    long 		m_length;
    long 		m_total;
    int 		m_reason;
    int 		m_type;
    char 		*m_buf;
    long 		m_event;
} ip_msg_t, *ip_msg_p;


struct ip_device {
    int 	ip_open;	/* is device opened? */
    int 	ip_rcvevent;    /* synchronization var flip interface */
    int		ip_bufevent;	/* synchronization var user-user copy */
    int 	ip_sndevent;    /* synchronization var flip and rpc */
    ip_msg_p	 ip_firstin;    /* queue with messages to be delivered */
    ip_msg_p 	*ip_lastin;
    int 	ip_inpktcnt;
    int		ip_ipdatacnt;
    char	ip_ipdata[MAXIPBUF];
};


static struct 	ip_device ip_dev[NIP]; 	/* devices for ctrl */

static pool_t 	ip_pool;
static pkt_t  	ip_pkt[NIPPKT];
static char   	ip_pktdata[NIPPKT * IPPKTSIZE];
static char   	ip_virdata[NIPPKT][IPVIRSIZE];

static int 	_fl_ipntw = -1;
static location nullloc;

static ip_msg_t 	msgbuf[MAXNPKT];
static ip_msg_p 	msg_free;


static void
fl_iparrived(pkt, loc)
    pkt_p pkt;
    location *loc;
{
    ip_msg_p msg;
    struct ip_device *fd;
    long totalvir, size;
    flip_hdr *fh;

    DPRINTF(1, ("fl_iparrived: enter\n"));

    fd = &ip_dev[0];		/* BUG! */
    if (!fd->ip_open) {
        pkt_discard(pkt);
        return;
    }
    if((msg = msg_free) == 0) {
        DPRINTF(0, ("fl_iparrived: no more buffers to store msg\n"));
        pkt_discard(pkt);
        return;
    }
    if(fd->ip_inpktcnt > MAXNPKT) {
        DPRINTF(0, ("fl_iparrived: no more buffers to store msg\n"));
        pkt_discard(pkt);
        return;
    }
    DPRINTF(1, ("fl_iparrived: enqueue pkt\n"));
    msg_free = msg->m_next;
    msg->m_next = 0;
    msg->m_pkt = pkt;
    msg->m_loc = *loc;
    *fd->ip_lastin = msg;
    fd->ip_lastin = &msg->m_next;
    fd->ip_inpktcnt++;

    DPRINTF(1, ("fl_iparrived: check on user data\n"));

    if(pkt->p_contents.pc_dstype == FL_DS_USER && pkt->p_contents.pc_virtual) {
    	DPRINTF(0, ("fl_iparrived: user data\n"));
        totalvir = pkt->p_contents.pc_totsize - pkt->p_contents.pc_dirsize;
	wakeup((caddr_t) &fd->ip_rcvevent);
        (void) fl_sleep((struct rpc_device *) pkt->p_contents.pc_dsident,
						(char *) &msg->m_event);
        proto_cur_header(fh, pkt, flip_hdr);
        while(totalvir > 0) {
            size = MIN(totalvir, MAXIPBUF);
            fd->ip_ipdatacnt = size;
            if(copyin((char *) pkt->p_contents.pc_virtual, fd->ip_ipdata,
							    (int) size) != 0) {
                pkt_discard(pkt);
            }
            fh->fh_length = pkt->p_contents.pc_dirsize + size -
                                                        sizeof(flip_hdr);
            wakeup((caddr_t) &fd->ip_bufevent);
            totalvir -= size;
            if(totalvir <= 0)
                break;
            (void) fl_sleep((struct rpc_device *) pkt->p_contents.pc_dsident,
							(char *) &msg->m_event);
            pkt->p_contents.pc_virtual += size;
            fh->fh_offset += size + pkt->p_contents.pc_dirsize -
                    sizeof(flip_hdr);
            pkt->p_contents.pc_dirsize = sizeof(flip_hdr);
        }
    } else {
    	DPRINTF(1, ("fl_iparrived: no user data; wakeup\n"));
	wakeup((caddr_t) &fd->ip_rcvevent);
    }
    DPRINTF(1, ("fl_iparrived: leave\n"));
}


/*ARGSUSED*/
static void fl_ipsend(dev, pkt, loc)
    int dev;
    pkt_p pkt;
    location *loc;
{
    fl_iparrived(pkt, loc);
}


/*ARGSUSED*/
static void fl_ipbcast(dev, pkt)
    int dev;
    pkt_p pkt;
{
    fl_iparrived(pkt, &nullloc);
}


static void del_inpkt(fd)
    struct ip_device *fd;
{
    ip_msg_p msg = fd->ip_firstin;

    if((fd->ip_firstin = msg->m_next) == 0) {
        fd->ip_lastin = &fd->ip_firstin;
    }
    fd->ip_inpktcnt--;
    pkt_discard(msg->m_pkt);
    msg->m_pkt = 0;
    msg->m_next = msg_free;
    msg_free = msg;
}


/*ARGSUSED*/
static void fl_ip_write(fd, fa)
    struct ip_device *fd;
    struct ip_args *fa;
{
    pkt_p pkt;
    int dirdata;
    int pkt_index;
    
    DPRINTF(1, ("fl_ip_write: enter\n"));

    PKT_GET(pkt, &ip_pool);
    if (pkt == 0) {
        fa->ip_status = -1;
        return;
    }
    if (fa->ip_dirsize != fa->ip_totsize || fa->ip_dirsize > IPVIRSIZE) {
        pkt_discard(pkt);
        fa->ip_status = -1;
        return;
    }

    proto_init(pkt);
    dirdata= pkt->p_admin.pa_size - pkt->p_contents.pc_offset;
    if (fa->ip_dirsize < dirdata)
	dirdata= fa->ip_dirsize;

    if(copyin(fa->ip_buffer, pkt_offset(pkt), dirdata) != 0)  {
	pkt_discard(pkt);
	fa->ip_status = -1;
	return;
    }
    pkt->p_contents.pc_dirsize = dirdata;
    pkt->p_contents.pc_totsize = dirdata;

    if (fa->ip_dirsize > dirdata)	/* We have to use virtual data. */
    {
	pkt_index= pkt - ip_pkt;
	assert(pkt_index >= 0 && pkt_index < NIPPKT);
	if(copyin(fa->ip_buffer+dirdata, ip_virdata[pkt_index], 
				(int) (fa->ip_dirsize - dirdata)) != 0)  {
	    pkt_discard(pkt);
	    fa->ip_status = -1;
	    return;
	}
	proto_set_virtual(pkt, 0, 0, (long)ip_virdata[pkt_index], 
						    fa->ip_dirsize - dirdata);
    }
    (void) proto_look_header(pkt, flip_hdr);
#if 0
    printf("-> pktswitch: ");
    pkt_print(pkt);
#endif
    pktswitch(pkt, _fl_ipntw, &fa->ip_loc);
    fa->ip_status = 0;

    DPRINTF(1, ("fl_ip_write: leave\n"));
}


static void fl_ip_read(fd, fa)
    struct ip_device *fd;
    struct ip_args *fa;
{
    ip_msg_p msg;
    struct flip_hdr *fh;
    long nbytes;

    DPRINTF(1, ("fl_ip_read: enter\n"));

    while((msg = fd->ip_firstin) == 0) {
    	DPRINTF(1, ("fl_ip_read: go to sleep\n"));
	sleep((caddr_t) &fd->ip_rcvevent, PZERO + 1);
    	DPRINTF(1, ("fl_ip_read: waken up; get msg\n"));
    }
    if(msg->m_pkt->p_contents.pc_dirsize + MAXIPBUF > fa->ip_totsize) {
	/* The direct data does not fit in the buffer supplied by the user.
	 * Give up.
	 */
	fa->ip_status = -1;
	del_inpkt(fd);
	return;
    }
    fa->ip_loc = msg->m_loc;
    proto_cur_header(fh, msg->m_pkt, flip_hdr);
    if(msg->m_pkt->p_contents.pc_virtual) {
    	DPRINTF(0, ("fl_ip_read: pkt contains user data\n"));
    	assert(msg->m_pkt->p_contents.pc_dstype == FL_DS_USER);
	/* The packet has to be fragmented; wake sender up so it can
	 * copy the date to the buffer rpcdata.
         */
	wakeup((caddr_t) &msg->m_event);
	sleep((caddr_t) &fd->ip_bufevent, PZERO+1);
	assert(msg->m_pkt->p_contents.pc_dirsize >= sizeof(flip_hdr));
	/* First fragment: copy dir data first. */
	if(copyout(pkt_offset(msg->m_pkt), fa->ip_buffer, 
		   msg->m_pkt->p_contents.pc_dirsize) != 0) {
	    fa->ip_status = -1;
	    del_inpkt(fd);
	    return;
	}
	nbytes = msg->m_pkt->p_contents.pc_dirsize;
	assert(fd->ip_ipdatacnt > 0);
	/* Copy the virtual data stored in rpcdata */
	if(copyout(fd->ip_ipdata, fa->ip_buffer + nbytes, 
		   fd->ip_ipdatacnt) != 0) {
	    fa->ip_status = -1;
	    del_inpkt(fd);
	    return;
	}
	nbytes += fd->ip_ipdatacnt;
	fa->ip_dirsize = fa->ip_totsize = nbytes;
	if(fh->fh_length + fh->fh_offset == fh->fh_total) {
	    /* Data of packet is completely copied; remove packet. */
	    del_inpkt(fd);
	}
    } else {
    	DPRINTF(1, ("fl_ip_read: pkt does not contain user data\n"));
	/* Easy case: one fragment message. */
	if(copyout(pkt_offset(msg->m_pkt), fa->ip_buffer, 
		   msg->m_pkt->p_contents.pc_dirsize) != 0)	{
	    fa->ip_status = -1;
	    del_inpkt(fd);
	    return;
	}
	fa->ip_dirsize = fa->ip_totsize = msg->m_pkt->p_contents.pc_dirsize;
	del_inpkt(fd);
    } 

    DPRINTF(1, ("fl_ip_read: leave\n"));
}


/*ARGSUSED*/
int fl_ip_ioctl(dev, cmd, fa, flag)
    dev_t dev;
    struct ip_args *fa;
{
    struct ip_device *fd;
    
    assert(minor(dev)-FIRST_IP >= 0 && minor(dev)-FIRST_IP < NIP);
    fd = &ip_dev[minor(dev)-FIRST_IP];
    switch (cmd) {
    case F_IP_WRITE:
	fl_ip_write(fd, fa);
	break;
    case F_IP_READ:
	fl_ip_read(fd, fa);
	break;
    default:
	return ENXIO;
    }
    return 0;
}


/*ARGSUSED*/
int fl_ip_open(dev, flags)
    dev_t dev;
    int flags;
{
    struct ip_device *fd;
    ip_msg_p m;
    
    if(_fl_ipntw == -1) {
	_fl_ipntw = flip_newnetwork("fl-ip", 0, 3, 0, fl_ipsend, fl_ipbcast,
				(void(*)()) 0, (void(*)()) 0, 0);
	pkt_init(&ip_pool, IPPKTSIZE, ip_pkt, NIPPKT, ip_pktdata,
							    (void(*)()) 0, 0L);
	msg_free = 0;
        for(m = msgbuf; m < msgbuf + MAXNPKT; m++) {
            m->m_pkt = 0;
            m->m_next = msg_free;
            msg_free = m;
        }
    }
    
    assert(minor(dev)-FIRST_IP >= 0 && minor(dev)-FIRST_IP < NIP);
    fd = &ip_dev[minor(dev)-FIRST_IP];
    assert(!fd->ip_open);
    fd->ip_open = 1;
    fd->ip_firstin= NULL;
    fd->ip_lastin= &fd->ip_firstin;
    fd->ip_inpktcnt= 0;
    return(0);
}


/*ARGSUSED*/
int fl_ip_close(dev, flags)
    dev_t dev;
    int flags;
{
    struct ip_device *fd;
    ip_msg_p msg, m;

    assert(minor(dev)-FIRST_IP >= 0 && minor(dev)-FIRST_IP < NIP);
    fd = &ip_dev[minor(dev)-FIRST_IP];
    assert(fd->ip_open);
    
    for(msg = fd->ip_firstin; msg != 0; msg = m) {
        if(msg->m_pkt != 0) {
	    pkt_discard(msg->m_pkt);
	    msg->m_pkt = 0;
        }
        m = msg->m_next;
        msg->m_next = msg_free;
        msg_free = msg;
    }
    fd->ip_firstin = 0;
    fd->ip_lastin = &fd->ip_firstin;
    fd->ip_inpktcnt = 0;
    
    fd->ip_open = 0;
    return(0);
}
