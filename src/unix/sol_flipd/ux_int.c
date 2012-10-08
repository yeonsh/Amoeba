/*	@(#)ux_int.c	1.1	96/02/27 11:49:53 */
#define ffs     string_ffs
#include "amoeba.h"
#include "stderr.h"
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "sys/flip/fragment.h"
#include "sys/debug.h"
#undef ffs
#include "ux_int.h"
#include "ux_rpc_int.h"
#include "ux_ip_int.h"
#include "ux_ctrl_int.h"
#include "kthread.h"
#include "sys/assert.h"
#include "sys/flip/flrpc_port.h"
#include "sys/flip/flrpc.h"
#undef timeout

#define threadp	unix_threadp
#include <sys/param.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/ksynch.h>
#include <sys/systm.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <errno.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#undef threadp

#include "fldefs.h"

INIT_ASSERT

#undef curthread

#define ETH_DATA 	(1500L - sizeof(fc_hdr_t) - sizeof(flip_hdr))

struct fleth fleth[MAXIFS];

static unsigned long seed;
static long ticks_per_sweep;
extern dev_info_t saved_dip;
extern state_p statetab;
int nifs = 0;
int timeoutval = -1;
int flip_unlinked = 0;


static void fliocdata(queue_t *, mblk_t *);
static void handle_link(queue_t *q, mblk_t *mp);

extern void handle_stat(queue_t *q, mblk_t *mp);
extern void handle_dump(queue_t *q, mblk_t *mp);
extern void handle_ctrl(queue_t *q, mblk_t *mp);


uint16 rand(void)
{
    /* I do not know where these magic numbers come from -- rvdp */
    return (uint16) (seed = seed * 1103515245 + 12345);
}


void rnd_init(void)
{
    /* initialise seed with number of clock ticks since the last reboot */
    drv_getparm(LBOLT, &seed);
}


/* The flip interface wants to sleep. For example, to wait until a flip
 * address is located. Return 1, if the sleep is terminated due to signal.
 * (see interface.c)
 */
int uxint_await(char *event, long val)
{
    assert(val == 0);

    printf("uxint_await: should NOT be called anymore!!\n");
    return(fl_sleep(0, event));
}


/* Wake the process that is sleeping in the flip interface up. */
int uxint_wakeup(char *event)
{
    printf("uxint_wakeup: should NOT be called anymore!!\n");
    fl_wakeup((caddr_t) event);
}



/* Run all the registered timers in the flip box. */
void fl_timer(void)
{
    register x;
    static int n = 0;

    mutex_enter(&flip_mutex);
    sweeper_run(SWEEPINTERVAL);		/* SWEEPINTERVAL is in msec */
    mutex_exit(&flip_mutex);
    timeoutval = timeout(fl_timer, (caddr_t) 0, ticks_per_sweep);
}



unix_init(void)
{
    DPRINTF(1, ("unix_init\n"));
    rnd_init();
    adr_init();
    netswitch_init();
    fleth_init();
    int_init();
    rpc_init();
    port_init();
    kid_init();
    /* SWEEPINTERVAL is in msec */
    ticks_per_sweep = drv_usectohz(SWEEPINTERVAL * 1000);
    timeoutval = timeout(fl_timer, (caddr_t) 0, ticks_per_sweep);
}


int flopen(
	queue_t *q,	/* pointer to the read queue */
	dev_t *devp,	/* pointer to major/minor device */
	int fflags,	/* file flags */
	int sflags,	/* stream open flags */
	cred_t *credp	/* pointer to a credentials struct */
)
{
    int r;
    minor_t minordev;
    int n;
    
    DPRINTF(1, ("flip: flopen\n"));

    minordev = getminor(*devp);

    /* allocate memory for private queue information */
    if (!q->q_ptr) {
	    if ((q->q_ptr = (struct fl_q_priv *) kmem_alloc(
			sizeof(struct fl_q_priv), KM_NOSLEEP)) == NULL) {
		return(ENOMEM);
	    }
	    WR(q)->q_ptr = q->q_ptr;
	    ((struct fl_q_priv *) q->q_ptr)->minor = minordev;
	    ((struct fl_q_priv *) q->q_ptr)->db = 0;
	    ((struct fl_q_priv *) q->q_ptr)->buf1 = 0;
	    ((struct fl_q_priv *) q->q_ptr)->pid = -1;
    }

    if (minordev >= FIRST_RPC && minordev <= LAST_RPC)  {
        /* allow clone open only for RPC device */
        if (sflags != CLONEOPEN)
	    return(ENXIO);
        for (minordev = FIRST_RPC; minordev < LAST_RPC; minordev++) {
            if (!rpc_dev[minordev].rpc_open) {
		break;
	    }
        }
        if (minordev >= LAST_RPC)
	    return(ENXIO);
        *devp = makedevice(getmajor(*devp), minordev);
	((struct fl_q_priv *) q->q_ptr)->minor = minordev;
	r = flrpc_open(q, devp, fflags, sflags, credp);
    } else if (minordev >= FIRST_IP && minordev <= LAST_IP) {
        /* IP code is not implemented yet */
        return(ENXIO);
	/*
	r = fl_ip_open(q, devp, fflags, sflags, credp);
	*/
    } else if (minordev >= FIRST_CTRL && minordev <= LAST_CTRL) {
	r = flctrl_open(q, devp, fflags, sflags, credp);
    } else {
	r = ENXIO;
    }
    qprocson(q);
    return(r);
}

/*
** User process did a I_LINK or I_PLINK ioctl(2). Handle it.
*/
static void handle_link(queue_t *q, mblk_t *mp)
{
    struct iocblk *iocp;
    struct linkblk *linkp;

    iocp = (struct iocblk *) mp->b_rptr;
    if (flip_unlinked == 1) {
	printf("FLIP: modunload before restarting!\n");
	iocp->ioc_count = 0;
	DB_TYPE(mp) = M_IOCNAK;
	qreply(q, mp);
	return;
    }
    if (nifs >= MAXIFS) {
	printf("FLIP: too many interfaces\n");
	iocp->ioc_count = 0;
	DB_TYPE(mp) = M_IOCNAK;
	qreply(q, mp);
	return;
    }
    linkp = (struct linkblk *) mp->b_cont->b_rptr;
    fleth[nifs].q = RD(linkp->l_qbot);
    fleth[nifs].fe_ntw = flip_newnetwork("Ethernet", nifs, ETH_WEIGHT, 0,
	    fleth_send, fleth_broadcast, (void (*)()) 0, (void (*)()) 0, 2);
    ff_newntw(fleth[nifs].fe_ntw, nifs, ETH_DATA, 32, 45, fl_req_credit,
	    failure);
    nifs++;
    iocp->ioc_count = 0;
    DB_TYPE(mp) = M_IOCACK;
    qreply(q, mp);
    flip_unlinked = 0;
    return;
}


int fluwsrv(queue_t *q)
{
	return(1);
}


/*
** FLIP Upper Write Put routine.
**
** This is the main entry point of the FLIP driver. It is called by all
** user process ioctl(2) and putmsg(2) calls.
*/
int fluwput(queue_t *q, mblk_t *mp)
{
    struct iocblk *iocp;
    struct copyreq *creqp;
    struct rpc_device *rpcfd;
    mblk_t *msg;
    minor_t minor;
    struct ctrl_args *ctrl_args;
    state_p st;
 
    switch (DB_TYPE(mp)) {
	case M_IOCTL:

	    /* This ios where we handle messages from ioctl(2). */

	    iocp = (struct iocblk *) mp->b_rptr;
	    switch (iocp->ioc_cmd) {
		case I_LINK:
		case I_PLINK:
		    handle_link(q, mp);
		    break;
		case I_UNLINK:
		case I_PUNLINK:
		    iocp->ioc_count = 0;
		    mp->b_datap->db_type = M_IOCACK;
		    qreply(q, mp);
		    flip_unlinked = 1;
		    return(0);
		    break;
		default:
		    snd_iocnak(q, mp, EINVAL);
		}
	    break;

	case M_PROTO:
	case M_PCPROTO:

	    /* This is where we handle messages from putmsg(2). */

	    minor = Q_PRIV(q)->minor;
	    if (minor >= FIRST_RPC && minor <= LAST_RPC)  {
		handle_rpc(q, mp);
	    } else if(minor >= FIRST_CTRL && minor <= LAST_CTRL) {
		ctrl_args = (struct ctrl_args *) DB_BASE(mp);
		switch (ctrl_args->ctrl_type) {
		    case FLCTRL_STAT:
			handle_stat(q, mp);
			break;
		    case FLCTRL_DUMP:
			handle_dump(q, mp);
			break;
		    case FLCTRL_CTRL:
			handle_ctrl(q, mp);
			break;
		}
	    } else {
		DB_TYPE(mp) = M_ERROR;
		qreply(q, mp);
		return;
	    }
	    break;

	case M_FLUSH:
	    minor = Q_PRIV(q)->minor;
	    rpcfd = &rpc_dev[minor - FIRST_RPC];
	    st = RPC_THREADSTATE((thread_t *) rpcfd);
	    mutex_enter(&flip_mutex);
	    port_remove(&st->st_pubport, &st->st_proc->ps_myaddr,
					    st->st_index, 1);
	    st->st_ackpkt = 0;
	    MAKE_IDLE(st);
	    /*
	    st->st_flag &= ~(F_SIGNAL | F_REASSEM | F_RETRANS);
	    */
	    st->st_flag = 0;
	    st->st_reassemble = -1;
	    st->st_rcnt = 0;

	    assert(NOCLIENT(st));
	    st->st_rhdr = 0;

	    if (*mp->b_rptr & FLUSHW) {
		flushq(q, FLUSHALL);
		*mp->b_rptr &= ~FLUSHW;
	    }
	    if (*mp->b_rptr & FLUSHR) {
		flushq(RD(q), FLUSHALL);
		qreply(q, mp);
	    } else {
		freemsg(mp);
	    }
	    mutex_exit(&flip_mutex);
	    break;
	default:
	    printf("fluwput: unknown message block arrived\n");
	    freemsg(mp);
    }
    return(0);
}


void snd_iocnak(queue_t *q, mblk_t *mp, int error)
{
	struct iocblk *iocbp;

	iocbp = (struct iocblk *)mp->b_rptr;
	mp->b_datap->db_type = M_IOCNAK;
	mp->b_wptr = mp->b_rptr + sizeof(struct iocblk);
	iocbp->ioc_error = error;
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	qreply(q, mp);
}


void snd_iocack(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocbp;

	iocbp = (struct iocblk *)mp->b_rptr;
	mp->b_datap->db_type = M_IOCACK;
	mp->b_wptr = mp->b_rptr + sizeof(struct iocblk);
	iocbp->ioc_error = 0;
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	qreply(q, mp);
}


int flclose(
    queue_t *q,		/* pointer to the read queue */
    int fflags,		/* file flags */
    cred_t *credp	/* pointer to a credentials struct */
)
{
    int r;
    minor_t minor;
    struct rpc_device *rpcfd;
    state_p st;

    /* Call close routines. */
    minor = Q_PRIV(q)->minor;
    qprocsoff(q);
    if (minor >= FIRST_RPC && minor <= LAST_RPC) {
	r =  flrpc_close(q, fflags, credp);
    } else if(minor >= FIRST_IP && minor <= LAST_IP) {
	/*
	r =  fl_ip_close(q, fflags, credp);
	*/
    } else if(minor >= FIRST_CTRL && minor <= LAST_CTRL) {
	r =  flctrl_close(q, fflags, credp);
    } else {
	r =  ENXIO;
    }
    if (q->q_ptr) {
	    kmem_free(q->q_ptr, sizeof(struct fl_q_priv));
	    q->q_ptr = 0;
    }
    return(r);
}
