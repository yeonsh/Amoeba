/*	@(#)ux_rpc_int.c	1.10	96/02/27 11:53:42 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "ux_int.h"
#include "ux_rpc_int.h"
#include "kthread.h"
#include "machdep.h"
#include "exception.h"
#include "sys/flip/flrpc_port.h"
#include "sys/flip/flrpc.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <time.h>
#include <signal.h>
#include <server/process/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <assert.h>
INIT_ASSERT

extern void rpc_initstate();
extern void rpc_stoprpc();
extern void sweeper_set();
extern long rpc_putrep();
extern long rpc_getreq();

#ifdef SECURITY
extern long secure_trans();
extern long secure_getreq();
extern long secure_putrep();
#endif /* SECURITY */

#define MAXBUF		100	/* Maximum buffers for incomming packets. */

static struct rpc_device rpc_dev[NRPC]; 	/* devices for flrpc */

static msg_t msgbuf[MAXBUF];
static msg_p msg_free;


/* Global Amoeba variables. */
thread_t *thread, *curthread;
static int sweep_needed;

static void rpc_watch(){
    register x;
    int i;
    
    x = spl6();
    if(sweep_needed) {
	sweep_needed = 0;
    	for(i=0; i < NRPC; i++) {
	    if(rpc_dev[i].rpc_timeout > 0) {
		sweep_needed = 1;
	    	if(--rpc_dev[i].rpc_timeout == 0) {
			rpc_dev[i].rpc_sleepflag = 0;
			fl_wakeup(rpc_dev[i].rpc_slpevent);
		}
	    }
	}
    }
    splx(x);
}


/* Sleep, but trickier because we have to catch signals. */
int fl_sleep(rpcfd, fe)
    struct rpc_device *rpcfd;
    char *fe;
{
    long oldsig = 0;
    int gotosleep = 1;
    int flags;

    flags = rpcfd->rpc_sleepflag;
    while(setjmp(&u.u_qsave)) {
	assert(rpcfd >= rpc_dev && rpcfd < rpc_dev + NRPC);
	assert(rpcfd->rpc_open);
	if(oldsig == 0) {
	    (void) rpc_sendsig(rpcfd);
	}
	if(flags & !rpcfd->rpc_sleepflag) {
	    gotosleep = 0;
	}
	oldsig |= u.u_procp->p_sig;
	if(oldsig & 0x100) {
	    return(1);
	}
	u.u_procp->p_sig = 0;
    }
    if(gotosleep) {
	sleep((caddr_t) fe, PZERO + 1);	
    }
    u.u_procp->p_sig = oldsig;
    return(0);
}



static int fl_wakeup(fe)
    char *fe;

{
    wakeup((caddr_t) fe);
}



/* The fragmentation layer has a packet that has to be sent away, but
 * it contains user data from process rpcfd. It wakes rpcfd up, so
 * that it can send the packet away and copy the data from user to
 * kernel space.
 */
void flux_store(pkt, last)
    pkt_p pkt;
    int last;
{
    struct rpc_device *rpcfd;

    rpcfd = (struct rpc_device *) (pkt->p_contents.pc_dsident);
    *rpcfd->rpc_lastout = pkt;
    pkt->p_admin.pa_next = 0;
    rpcfd->rpc_lastout = &pkt->p_admin.pa_next;
    rpcfd->rpc_outcomplete = last;
    fl_wakeup((char *) &rpcfd->rpc_sndevent);
}


/* Rpcfd is block in the somewhere down in the flip-box waiting until
 * the fragmentation layer fragments the message in packets and tells
 * it to send a packet.
 */
pkt_p flux_getpkt()
{
    struct rpc_device *rpcfd;
    int pid;
    pkt_p pkt;

    pid = u.u_procp->p_pid;
    for(rpcfd = rpc_dev; rpcfd < rpc_dev + NRPC; rpcfd++) {
	if(rpcfd->rpc_open && rpcfd->rpc_pid == pid) {
	    if(rpcfd->rpc_outcomplete && rpcfd->rpc_firstout == 0) {
		rpcfd->rpc_outcomplete = 0;
		return(0);
	    }
	    while(rpcfd->rpc_firstout == 0 && !rpcfd->rpc_outcomplete) {
		(void) fl_sleep(rpcfd, (char *) &rpcfd->rpc_sndevent);
	    }
	    if(rpcfd->rpc_firstout) {
		pkt = rpcfd->rpc_firstout;
		if((rpcfd->rpc_firstout = pkt->p_admin.pa_next) == 0) {
		    rpcfd->rpc_lastout = &rpcfd->rpc_firstout;
		}
		pkt->p_admin.pa_next = 0;
		return(pkt);
	    } else return(0);
	}
    }
    return(0);
}


/* The fragmentation layer couldn't send the message away. */
void flux_failure(pkt)
    pkt_p pkt;
{
    struct rpc_device *rpcfd;

    if(pkt->p_contents.pc_dsident != 0) {
	/* A process running on this cpu was trying to send the packet.
	 * Wake the process up.
	 */
    	rpcfd = (struct rpc_device *) (pkt->p_contents.pc_dsident);
    	rpcfd->rpc_outcomplete = 1;
    	fl_wakeup((char *) &rpcfd->rpc_sndevent);
    }
    pkt_discard(pkt);
}


/* Check if there is data that have to be copied from the kernel to
 * user space. If so, copy data in pieces.
 */
static void uxrpc_getdata(rpcfd)
    struct rpc_device *rpcfd;
{
    msg_p msg;
    pkt_p pkt;
    struct rpc_device *snd_rpcfd;
    f_size_t l, size;

    while(rpcfd->rpc_firstin != 0) {
	msg = rpcfd->rpc_firstin;
	if((rpcfd->rpc_firstin = msg->m_next) == 0) {
	    rpcfd->rpc_lastin = &rpcfd->rpc_firstin;
	}
	pkt = msg->m_pkt;
	l = msg->m_length;
	if(pkt->p_contents.pc_virtual != 0 && pkt->p_contents.pc_dstype ==
	   FL_DS_USER) {
	    snd_rpcfd = (struct rpc_device *) pkt->p_contents.pc_dsident;
	    size = MIN(l, pkt->p_contents.pc_dirsize);
	    l = l - size;
	    if(copyout(pkt_offset(pkt), msg->m_buf, (int) size) != 0) {
		printf("uxrpc_getdata: copy failed\n");
	    }
	    msg->m_buf += size;
	    while(rpcfd->rpc_rpcdatacnt > 0) {
		size = MIN(l, rpcfd->rpc_rpcdatacnt);
		l = l - size;
		if(copyout(rpcfd->rpc_rpcdata, msg->m_buf, size) != 0)  {  
		    printf("uxrpc_getdata: copy failed\n");
		}
		msg->m_buf += size;
		fl_wakeup((char *) &snd_rpcfd->rpc_bufevent);
		(void) fl_sleep(rpcfd, (char *) &rpcfd->rpc_bufevent);
	    }
	} else {
	    size = MIN(l, pkt->p_contents.pc_dirsize);
	    l = l - size;
	    if(copyout(pkt_offset(pkt), msg->m_buf, size) != 0) {
		printf("uxrpc_getdata: copy failed\n");
	    }
	    if (pkt->p_contents.pc_virtual != 0)
	    {
		if(copyout((char *)pkt->p_contents.pc_virtual, msg->m_buf +
			   size, (int) l) != 0) {
		    printf("uxrpc_getdata: copy failed\n");
		}
	    }
	}
	pkt_discard(msg->m_pkt);
	msg->m_pkt = 0;
	msg->m_next = msg_free;
	msg_free = msg;
	rpcfd->rpc_inpktcnt--;
	assert(rpcfd->rpc_inpktcnt >= 0);
    }
}


/* Buffer data for the process 'rpcfd'. */
void uxrpc_buffer(rpcfd, pkt, buf, n)
    struct rpc_device *rpcfd;
    pkt_p pkt;
    char *buf;
    f_size_t n;
{
    msg_p msg;
    long size, totalvir;
    struct rpc_device *snd_rpcfd;
    int first;
    
    assert(rpcfd >= rpc_dev && rpcfd < rpc_dev+NRPC);
    if (!rpcfd->rpc_open) {
	printf("FLIP: interrupt to closed device???\n");
	pkt_discard(pkt);
	return;
    }
    if((msg = msg_free) == 0) {
	printf("uxrpc_buffer: no more buffers to store msg\n");
	pkt_discard(pkt);
	return;
    }

    assert (msg >= msgbuf && msg < msgbuf+MAXBUF);

    /* Grap a message structure to store pkt temporarily, so
     * that the process going with rpcfd can get the data and
     * copy the datat to its user space.
     */
    msg_free = msg->m_next;
    msg->m_next = 0;
    msg->m_pkt = pkt;
    msg->m_length = n;
    msg->m_buf = buf;

    *rpcfd->rpc_lastin = msg;
    rpcfd->rpc_lastin = &msg->m_next;
    rpcfd->rpc_inpktcnt++;

    /* check if there is any data from user space in this packet */
    if(pkt->p_contents.pc_dstype == FL_DS_USER && pkt->p_contents.pc_virtual) {
	first = 1;
	totalvir = pkt->p_contents.pc_totsize - pkt->p_contents.pc_dirsize;
	snd_rpcfd = (struct rpc_device *) pkt->p_contents.pc_dsident;
	while(totalvir > 0) {
	    size = MIN(totalvir, MAXRPCBUF);
	    rpcfd->rpc_rpcdatacnt = size;

	    /* Copy virtual data in pieces from user space to kernel space. */
	    if(copyin((char *) pkt->p_contents.pc_virtual, rpcfd->rpc_rpcdata,
							(int) size) != 0)  {
		printf("uxrpc_buffer: copyin failed\n");
		pkt_discard(pkt);
	    }
	    totalvir -= size;

	    /* wakeup the destination, so that it can get the data */
	    if(first) {
		first = 0;
		fl_wakeup(rpcfd->rpc_slpevent);
	    } else {
		fl_wakeup((char *) &rpcfd->rpc_bufevent);
	    }
	    (void) fl_sleep(rpcfd, (char *) &snd_rpcfd->rpc_bufevent);
	}
	rpcfd->rpc_rpcdatacnt = 0;
	fl_wakeup((char *) &rpcfd->rpc_bufevent);
    }
    if(rpcfd->rpc_sleepflag) {
	fl_wakeup(rpcfd->rpc_slpevent);
    }
}


int uxrpc_userthread()
{
    /* there are no kernel threads in the unix driver so return 1. */

    return(1);
}


void uxrpc_progerror()
{
    /* abort the user process responsible for this call */

    nosys();
}


unsigned long uxrpc_getmilli()
{
    unsigned long msec = time.tv_sec * 1000 + time.tv_usec /1000;

    return(msec);
}

/*ARGSUSED*/
int uxrpc_getpid(rpcfd)
    struct rpc_device *rpcfd;
{
    /* Treat all process as one flip entity. We are not going to
     * implement process migration under unix.
     */

    return(0);
}


/*ARGSUSED*/
int THREADSLOT(rpcfd)
    struct rpc_device *rpcfd;
{
    /* Each minor device is one thread; this is unix. */

    return(rpcfd - rpc_dev);
}


/*ARGSUSED*/
int uxrpc_putsig(rpcfd, signal)
    struct rpc_device *rpcfd;
    signum signal;
{
    assert(rpcfd->rpc_open);
    psignal(rpcfd->rpc_proc, SIGEMT);
}


int uxrpc_wakeup(rpcfd, event)
    struct rpc_device *rpcfd;
    char *event;
{
    rpcfd->rpc_sleepflag = 0;
    fl_wakeup(event);
}


/* Sleep until a wakeup on event or the timer runs out.  While the process is
 * sleeping, it wakes up once in while to copy data from the kernel to the 
 * user's address space.
 */
int uxrpc_await(rpcfd, event, val)
    struct rpc_device *rpcfd;
    char *event;
    long val;
{
    rpcfd->rpc_sleepflag = 1;
    rpcfd->rpc_slpevent = event;
    rpcfd->rpc_timeout = val;
    if(val > 0) sweep_needed = 1;
    do {
	uxrpc_getdata(rpcfd);
	if(rpcfd->rpc_sleepflag) {
	    if(fl_sleep(rpcfd, event)) {
		printf("uxrpc_await returns 1\n");
		return(1);
	    }
	}
    } while(rpcfd->rpc_sleepflag);
    uxrpc_getdata(rpcfd);
    rpcfd->rpc_timeout = 0;
    return(0);
}
    

/* Remove incomming packets. */
void uxrpc_cleanup(rpcfd)
    struct rpc_device *rpcfd;
{
    msg_p msg, m;

    for(msg = rpcfd->rpc_firstin; msg != 0; msg = m) {
        rpcfd->rpc_inpktcnt--;
	assert(msg->m_pkt != 0);
	pkt_discard(msg->m_pkt);
	msg->m_pkt = 0;
	m = msg->m_next;
	msg->m_next = msg_free;
	msg_free = msg;
    }
    assert(rpcfd->rpc_inpktcnt == 0);
    rpcfd->rpc_firstin = 0;
    rpcfd->rpc_lastin = &rpcfd->rpc_firstin;
    rpcfd->rpc_inpktcnt = 0;
}


static void flrpc_trans(rpcfd, fa)
    struct rpc_device *rpcfd;
    struct rpc_args *fa;
{
    if(rpcfd->rpc_pid != u.u_procp->p_pid) { 
	/* a child using the parents rpcfd? */
	fa->rpc_error = 0;
	return;
    } else fa->rpc_error = 1;
    
    curthread = (thread_t *) rpcfd;
    rpcfd->rpc_inpktcnt = 0;
#ifdef SECURITY
    fa->rpc_status = secure_trans(&fa->rpc_hdr1, fa->rpc_buf1, fa->rpc_cnt1,
			      &rpcfd->rpc_rhdr, fa->rpc_buf2, fa->rpc_cnt2); 
#else
    fa->rpc_status = rpc_trans(&fa->rpc_hdr1, fa->rpc_buf1, fa->rpc_cnt1,
			      &rpcfd->rpc_rhdr, fa->rpc_buf2, fa->rpc_cnt2); 
#endif
    fa->rpc_hdr2 = rpcfd->rpc_rhdr;
}


static void flrpc_getreq(rpcfd, fa)
    struct rpc_device *rpcfd;
    struct rpc_args *fa;
{
    if(rpcfd->rpc_pid != u.u_procp->p_pid) { 
	/* a child using the parents rpcfd? */
	fa->rpc_error = 0;
	return;
    } else fa->rpc_error = 1;
    curthread = (thread_t *) rpcfd;
    rpcfd->rpc_inpktcnt = 0;
    rpcfd->rpc_rhdr = fa->rpc_hdr1;
    fa->rpc_status = rpc_getreq(&rpcfd->rpc_rhdr, fa->rpc_buf1, fa->rpc_cnt1);
    fa->rpc_hdr1 = rpcfd->rpc_rhdr;
}


static void flrpc_putrep(rpcfd, fa)
    struct rpc_device *rpcfd;
    struct rpc_args *fa;
{
    if(rpcfd->rpc_pid != u.u_procp->p_pid) { 
	/* a child using the parents rpcfd? */
	fa->rpc_error = 0;
	return;
    } else fa->rpc_error = 1;

    curthread = (thread_t *) rpcfd;
    fa->rpc_status = rpc_putrep(&fa->rpc_hdr1, fa->rpc_buf1, fa->rpc_cnt1);
}


/*
 * Check that the specified user buffer actually exists and is valid
 * Return 0 on success and non-zero otherwise
 */

static int
probe_ubuffer(buf, cnt)
bufptr buf;
f_size_t cnt;
{
    char test_byte;

    return copyin(buf, &test_byte, 1) ||
	   copyin((char *) buf + cnt, &test_byte, 1);
}


/*ARGSUSED*/
int flrpc_ioctl(dev, cmd, fa, flag)
    dev_t dev;
    struct rpc_args *fa;
{
    struct rpc_device *rpcfd;
    
    assert(minor(dev) >= FIRST_RPC && minor(dev) <= LAST_RPC);
    rpcfd = &rpc_dev[minor(dev)];
    if (fa->rpc_cnt1 != 0 && probe_ubuffer(fa->rpc_buf1, fa->rpc_cnt1))
    {
	uxrpc_progerror();
	return EFAULT;
    }
    switch (cmd) {
    case F_TRANS:
	if (fa->rpc_cnt2 != 0 && probe_ubuffer(fa->rpc_buf2, fa->rpc_cnt2))
	{
	    uxrpc_progerror();
	    return EFAULT;
	}
	flrpc_trans(rpcfd, fa);
	break;
    case F_GETREQ:
	flrpc_getreq(rpcfd, fa);
	break;
    case F_PUTREP:
	flrpc_putrep(rpcfd, fa);
	break;
    default:
	return ENXIO;
    }
    return 0;
}

static initfd(rpcfd)
    struct rpc_device *rpcfd;
{
    if (rpcfd->rpc_open) return(0);
    rpcfd->rpc_open = 1;
    rpcfd->rpc_proc = u.u_procp;
    rpcfd->rpc_pid = u.u_procp->p_pid;
    rpcfd->rpc_firstin = 0;
    rpcfd->rpc_lastin = &rpcfd->rpc_firstin;
    rpcfd->rpc_firstout = 0;
    rpcfd->rpc_lastout = &rpcfd->rpc_firstout;
    rpcfd->rpc_timeout = 0;
    rpcfd->rpc_inpktcnt = 0;
    return(1);
}


static int initstate;

/*ARGSUSED*/
int flrpc_open(dev, flags)
    dev_t dev;
    int flags;
{
    struct rpc_device *fd;
    msg_p m;
    int i;

    assert(minor(dev) >= FIRST_RPC && minor(dev) <= LAST_RPC);
    fd = &rpc_dev[minor(dev)];
    switch (initstate) {
    case 0:
        initstate = 1;
        sweeper_set(rpc_watch, 0L, (interval) SWEEPINTERVAL, 0);
	thread = (thread_t *) rpc_dev;
	for(i=0; i < NRPC; i++) rpc_dev[i].rpc_open = 0;
	msg_free = 0;
	for(m = msgbuf; m < msgbuf + MAXBUF; m++) {
	    m->m_pkt = 0;
	    m->m_next = msg_free;
	    msg_free = m;
	}
        initstate = 2;
        wakeup((caddr_t) &initstate);
        break;
    case 1:
        do {
            sleep((caddr_t) &initstate, PSLEP + 1);
	} while (initstate == 1);
    }
    if(initfd(fd)) {
	rpc_initstate(fd);
	return(0);
    } 
    return(EIO);
}


/*ARGSUSED*/
int flrpc_close(dev, flags)
    dev_t dev;
    int flags;
{
    struct rpc_device *fd;
    pkt_p pkt, p;
    
    assert(minor(dev) >= FIRST_RPC && minor(dev) <= LAST_RPC);

    fd = &rpc_dev[minor(dev)];
    assert(fd->rpc_open);
    rpc_stoprpc(fd);
    rpc_cleanstate(fd);
    for(pkt = fd->rpc_firstout; pkt != 0; pkt = p) {
	p = pkt->p_admin.pa_next;
	pkt->p_admin.pa_next = 0;
	pkt_discard(pkt);
    }
    fd->rpc_firstout = 0;
    fd->rpc_lastout = &fd->rpc_firstout;
    uxrpc_cleanup(fd); 
    fd->rpc_open = 0;
    return(0);
}
