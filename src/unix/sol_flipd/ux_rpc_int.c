/*	@(#)ux_rpc_int.c	1.1	96/02/27 11:49:54 */
#include "amoeba.h"
#include "stderr.h"
#define ffs     string_ffs
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#undef ffs
#include "ux_int.h"
#include "ux_rpc_int.h"
#include "kthread.h"
#include "machdep.h"
#include "exception.h"
#include "module/rpc.h"
#include "assert.h"
#include "sys/debug.h"
#include "sys/flip/flrpc_port.h"
#include "sys/flip/flrpc.h"
#undef	timeout

#define threadp	unix_threadp
#include <sys/param.h>
#include <sys/types.h>
#include <server/process/proc.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#undef threadp

#include "fldefs.h"

INIT_ASSERT

#undef curthread

struct rpc_device rpc_dev[NRPC];      /* devices for rpc */

struct rpc_info_t rpc_info[NRPC];


#define MAXBUF		100	/* Maximum buffers for incomming packets. */

static volatile int initstate;



/* Global Amoeba variables. */
thread_t *thread, *curthread;
static volatile int sweep_needed;

void handle_rpc(queue_t *q, mblk_t *mp);

extern dev_info_t saved_dip;
extern state_p statetab;


static void rpc_watch(long id){
    register x;
    int i;
    
    if(sweep_needed) {
	sweep_needed = 0;
    	for(i=0; i < NRPC; i++) {
	    if(rpc_dev[i].rpc_timeout > 0) {
		sweep_needed = 1;
	    	if(--rpc_dev[i].rpc_timeout <= 0) {
			rpc_dev[i].rpc_sleepflag = 0;
			DPRINTF(1, ("rpc_watch: calling fl_wakeup(rpc_dev[i].rpc_slpevent)\n"));
			fl_wakeup(rpc_dev[i].rpc_slpevent);
		}
	    }
	}
    }
}



kcondvar_t *get_cv(caddr_t event)
{
	static struct cv_list_t *head = 0;
	static int count = 0;
	struct cv_list_t *ptr;
	struct cv_list_t *new;

	for (ptr = head; ptr != 0; ptr = ptr->next) {
		if (ptr->event == event)
			return(&ptr->cv);
	}
	if ((new = (struct cv_list_t *) kmem_alloc(sizeof(struct cv_list_t),
				KM_NOSLEEP)) == NULL) {
		cmn_err(CE_PANIC, "FLIP: get_cv: out of memory\n");
	}
	new->event = event;
	cv_init(&new->cv, "condvar", CV_DRIVER, NULL);
	new->next = head;
	head = new;
	DPRINTF (0, ("FLIP: get_cv: count is %d\n", ++count));
	return(&new->cv);
}


/* Sleep, but trickier because we have to catch signals. */
int fl_sleep(struct rpc_device *rpcfd, char *fe)
{
    int rval;
    state_p st;
    struct rpc_args *rpc_args;
    char *type;
    char *state;
    long timeout;

    cmn_err(CE_PANIC, "fl_sleep: should not be called anymore");

    DPRINTF(0, ("fl_sleep: sleep on %x\n", fe));
    timeout = drv_usectohz(5000 * 1000);
    mutex_exit(&flip_mutex);
    mutex_enter(&sleep_mutex);
    rval = cv_timedwait_sig(get_cv(fe), &sleep_mutex, timeout);
    mutex_exit(&sleep_mutex);
    mutex_enter(&flip_mutex);
    if (rval == 0) {
	/* interrupted by signal */
	DPRINTF(0, ("fl_sleep: cv_timedwait_sig interrupted by signal\n"));
	return(1);
    }
    if (rval == -1) {
	/* timeout */
	DPRINTF(0, ("fl_sleep: cv_timedwait_sig timed out\n"));
	return(1);
    }
    DPRINTF(1, ("fl_sleep: woke up on %x\n", fe));
    return(0);
}



int fl_wakeup(char *fe)
{
    DPRINTF(0, ("fl_wakeup on %x\n", fe));
    mutex_enter(&sleep_mutex);
    cv_broadcast(get_cv(fe));
    mutex_exit(&sleep_mutex);
    /* wakeup((caddr_t) fe); */
    return(0);
}


void handle_rpc(queue_t *q, mblk_t *mp)
{
    mblk_t *mp_data;
    struct rpc_args *rpc_args;
    struct rpc_device *rpcfd;
    minor_t minor;
    state_p st;

    rpc_args = (struct rpc_args *) DB_BASE(mp);

    mutex_enter(&flip_mutex);

    if (Q_PRIV(q)->pid == -1) {
	/* queue just opened, save process ID */
	Q_PRIV(q)->pid = rpc_args->rpc_pid;
    } else if (Q_PRIV(q)->pid != rpc_args->rpc_pid) {
	/* process has wrong process ID, send NACK */
	DB_TYPE(mp) = M_PROTO;
	rpc_args->rpc_error = 0;
	mutex_exit(&flip_mutex);
	qreply(q, mp);
	return;
    }

    minor = Q_PRIV(q)->minor;
    rpcfd = &rpc_dev[minor - FIRST_RPC];

    mp_data = mp->b_cont;
    mp->b_cont = 0;
    rpc_args->rpc_buf1 = (char *) DB_BASE(mp_data);

    rpcfd->rpc_pid = rpc_args->rpc_pid;
    assert(mp_data->b_cont == 0);
    rpcfd->mp = mp;
    rpcfd->mp_data = mp_data;
    rpcfd->queue = q;
    rpc_args->rpc_error = 1;

    curthread = (thread_t *) rpcfd;

    switch (rpc_args->rpc_type) {
	case FLRPC_TRANS:
	    if ((mp->b_cont = allocb(rpc_args->rpc_cnt2, BPRI_MED)) == NULL) {
		printf("FLIP: handle_rpc: out of message blocks\n");
		rpc_args->rpc_error = 0;
		mutex_exit(&flip_mutex);
		qreply(q, mp);
		return;
	    }
	    rpc_args->rpc_buf2 = (char *) DB_BASE(mp->b_cont);
	    mp->b_cont->b_rptr = DB_BASE(mp->b_cont);
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + rpc_args->rpc_cnt2;

#ifdef SECURITY
	    rpc_args->rpc_status = secure_trans(&rpc_args->rpc_hdr1,
		    rpc_args->rpc_buf1, rpc_args->rpc_cnt1,
		    &rpcfd->rpc_rhdr, rpc_args->rpc_buf2,
		    rpc_args->rpc_cnt2);
#else
	    rpc_args->rpc_status = rpc_trans(&rpc_args->rpc_hdr1,
		    rpc_args->rpc_buf1, rpc_args->rpc_cnt1,
		    &rpcfd->rpc_rhdr, rpc_args->rpc_buf2,
		    rpc_args->rpc_cnt2);
#endif
	    rpc_args->rpc_hdr2 = rpcfd->rpc_rhdr;
	    mutex_exit(&flip_mutex);
	    return;
	    break;
	case FLRPC_GETREQ:
	    if ((mp->b_cont = allocb(rpc_args->rpc_cnt1, BPRI_MED)) == NULL) {
		printf("FLIP: handle_rpc: out of message blocks\n");
		rpc_args->rpc_error = 0;
		rpc_args->rpc_status = RPC_FAILURE;
		mutex_exit(&flip_mutex);
		qreply(q, mp);
		return;
	    }
	    rpc_args->rpc_buf1 = (char *) DB_BASE(mp->b_cont);
	    mp->b_cont->b_rptr = DB_BASE(mp->b_cont);
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + rpc_args->rpc_cnt1;
	    rpcfd->rpc_rhdr = rpc_args->rpc_hdr1;
	    *(int *) (rpc_args->rpc_buf1) = rpc_args->rpc_pid;
	    rpc_args->rpc_status = rpc_getreq(&rpcfd->rpc_rhdr,
		    rpc_args->rpc_buf1, rpc_args->rpc_cnt1);
	    rpc_args->rpc_hdr1 = rpcfd->rpc_rhdr;
	    mutex_exit(&flip_mutex);
	    return;
	    break;
	case FLRPC_PUTREP:
	    rpc_args->rpc_status = rpc_putrep(&rpc_args->rpc_hdr1,
		    rpc_args->rpc_buf1, rpc_args->rpc_cnt1);
	    mutex_exit(&flip_mutex);
	    return;
	    break;
	default:
	    mutex_exit(&flip_mutex);
	    printf("unknown M_PROTO received\n");
	    DB_TYPE(mp) = M_PROTO;
	    rpc_args->rpc_error = 0;
	    qreply(q, mp);
	    return;
    }
}


/* The fragmentation layer couldn't send the message away. */
void flux_failure(pkt_p pkt)
{
    struct rpc_device *rpcfd;

    printf("flux_failure\n");
    if(pkt->p_contents.pc_dsident != 0) {
	/* A process running on this cpu was trying to send the packet.
	 * Wake the process up.
	 */
    	rpcfd = (struct rpc_device *) (pkt->p_contents.pc_dsident);
    	rpcfd->rpc_outcomplete = 1;
	DPRINTF(1, ("flux_failure: fl_wakeup((char *) &rpcfd->rpc_sndevent)\n"));
    	fl_wakeup((char *) &rpcfd->rpc_sndevent);
    }
    pkt_discard(pkt);
}


int uxrpc_userthread()
{
    /* there are no kernel threads in the unix driver so return 1. */

    DPRINTF(1, ("uxrpc_userthread\n"));
    return(1);
}


void uxrpc_progerror()
{
    /* abort the user process responsible for this call */

    printf("uxrpc_progerror\n");
    nosys();
}


extern struct timespec hrestime;
 
unsigned long uxrpc_getmilli()
{
    unsigned long msec = hrestime.tv_sec * 1000 + hrestime.tv_nsec / 1000000;

    return(msec);
}

/*ARGSUSED*/
int uxrpc_getpid(struct rpc_device *rpcfd)
{
    /* Treat all processes as one flip entity. We are not going to
     * implement process migration under unix.
     */

    DPRINTF(1, ("uxrpc_getpid\n"));
    return(0);
}


/*ARGSUSED*/
/* Each minor device is one thread; this is unix. */
int THREADSLOT(struct rpc_device *rpcfd)
{
    return(rpcfd - rpc_dev);
}


/*ARGSUSED*/
int uxrpc_putsig(struct rpc_device *rpcfd, signum signal)
{
    assert(rpcfd->rpc_open);
    if (proc_signal(rpcfd->proc_ref, 7) != 0) {
	/* process has gone, remove handle */
	proc_unref(rpcfd->proc_ref);
	return(1);
    }
    return(0);
}


int uxrpc_wakeup(struct rpc_device *rpcfd, char *event)
{
    printf("uxrpc_wakeup\n");
    rpcfd->rpc_sleepflag = 0;
    DPRINTF(0, ("uxrpc_wakeup: calling  fl_wakeup(event)\n"));
    fl_wakeup(event);
}


/* Sleep until a wakeup on event or the timer runs out.  While the process is
 * sleeping, it wakes up once in while to copy data from the kernel to the 
 * user's address space.
 */
int uxrpc_await(struct rpc_device *rpcfd, char *event, long val)
{
    printf("uxrpc_await\n");
    rpcfd->rpc_sleepflag = 1;
    rpcfd->rpc_slpevent = event;
    rpcfd->rpc_timeout = val;
    if(val > 0) sweep_needed = 1;
    do {
	if(rpcfd->rpc_sleepflag) {
	    DPRINTF(1, ("uxrpc_await: calling if(fl_sleep(rpcfd, event))\n"));
	    if(fl_sleep(rpcfd, event)) {
		return(1);
	    }
	}
    } while(rpcfd->rpc_sleepflag);
    rpcfd->rpc_timeout = 0;
    return(0);
}
    

static initfd(struct rpc_device *rpcfd)
{
    DPRINTF(1, ("initfd\n"));
    if (rpcfd->rpc_open) {
	return(0);
    }
    rpcfd->rpc_open = 1;
    rpcfd->rpc_firstin = 0;
    rpcfd->rpc_lastin = &rpcfd->rpc_firstin;
    rpcfd->rpc_firstout = 0;
    rpcfd->rpc_lastout = &rpcfd->rpc_firstout;
    rpcfd->rpc_timeout = 0;
    rpcfd->rpc_inpktcnt = 0;
    rpcfd->mp = 0;
    rpcfd->mp_data = 0;
    return(1);
}


/*ARGSUSED*/
int flrpc_open(
        queue_t *q,     /* pointer to the read queue */
        dev_t *devp,    /* pointer to major/minor device */
        int fflags,     /* file flags */
        int sflags,     /* stream open flags */
        cred_t *credp   /* pointer to a credentials struct */
)
{
    struct rpc_device *rpcfd;
    int i;
    int minordev;

    DPRINTF(1, ("flrpc_open\n"));
    minordev = getminor(*devp);
    assert(minordev >= FIRST_RPC && minordev <= LAST_RPC);
    rpcfd = &rpc_dev[minordev - FIRST_RPC];
    rpc_info[minordev - FIRST_RPC].q = WR(q);
    DPRINTF(1, ("flrpc_open: queue is %x\n", q));
    DPRINTF(1, ("flrpc_open: saved queue is %x\n", WR(q)));
    mutex_enter(&flopen_mutex);
    rpcfd->proc_ref = proc_ref();
    if (initstate == 0) {
        sweeper_set(rpc_watch, 0L, (interval) SWEEPINTERVAL, 0);
	thread = (thread_t *) rpc_dev;
	for (i=0; i < NRPC; i++)
		rpc_dev[i].rpc_open = 0;
        initstate = 1;
    }
    mutex_exit(&flopen_mutex);
    if (initfd(rpcfd)) {
	rpc_initstate((thread_t *) rpcfd);
	return(0);
    } 
    return(EIO);
}


/*ARGSUSED*/
int flrpc_close(queue_t *q, int fflags, cred_t *credp)
{
    struct rpc_device *rpcfd;
    pkt_p pkt, p;
    minor_t minor;
    state_p st;
    
    mutex_enter(&flip_mutex);
    minor = ((struct fl_q_priv *)q->q_ptr)->minor;
    assert(minor >= FIRST_RPC && minor <= LAST_RPC);
    rpcfd = &rpc_dev[minor - FIRST_RPC];
    assert(rpcfd->rpc_open);
    proc_unref(rpcfd->proc_ref);
    rpc_cleanstate(rpcfd);
    st = RPC_THREADSTATE((thread_t *) rpcfd);
    st->st_rhdr = 0;
    for(pkt = rpcfd->rpc_firstout; pkt != 0; pkt = p) {
	p = pkt->p_admin.pa_next;
	pkt->p_admin.pa_next = 0;
	pkt_discard(pkt);
    }
    rpcfd->rpc_firstout = 0;
    rpcfd->rpc_lastout = &rpcfd->rpc_firstout;
    rpcfd->rpc_open = 0;
    rpcfd->mp = 0;
    rpcfd->mp_data = 0;
    Q_PRIV(q)->pid = -1;
    mutex_exit(&flip_mutex);
    return(0);
}
