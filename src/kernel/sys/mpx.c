/*	@(#)mpx.c	1.15	96/02/27 14:22:38 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** mpx - scheduling and thread/process management.
*/

/*
** Enable mpx specific definitions in kthread.h:
*/
#define MPX
#include <string.h>
#include "amoeba.h"
#include "global.h"
#include "machdep.h"
#include "mmudep.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "process/proc.h"
#include "table.h"
#include "exception.h"
#include "kthread.h"
#include "map.h"
#include "sys/proto.h"
#include "module/mutex.h"
#include "sys/flip/measure.h"

#ifdef NO_MPX_REGISTER
#ifdef FLIPGRP
#include "group.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "sys/flip/group.h"
#else
#define grp_sendsig(t)	0
#endif /* FLIPGRP */
#endif

#define RESSWEEP	100L		/* milliseconds between mpxsweeps */

/* Start of later multi priority scheduling */
#define PRIO_KERNEL	0
#define PRIO_USER	1

#define SIGFLAGS (TDS_USIG|TDS_HSIG|TDS_SRET)

#ifdef ROMKERNEL
#define MAX_INTLIST 32
#else
#define MAX_INTLIST 2048
#endif

#ifndef KSTK_ALLOC
#define KSTK_ALLOC	getblk
#define KSTK_FREE	relblk
#endif

#ifndef KSTK_DEFAULT_SIZE
#define KSTK_DEFAULT_SIZE	4000
#endif

#ifdef	NOPROC
#define	NPROC	1	/* kernel process */
#else
#ifndef	NPROC
#define	NPROC	32
#endif	/* NPROC */
#endif	/* NOPROC */

#ifndef NTHREAD
#ifndef NOPROC
#define NTHREAD	256
#else
#define NTHREAD 64
#endif
#endif

uint16 nproc = NPROC;
uint16 totthread = NTHREAD;

       struct process *	processarray;	/* All process structures */
static struct process *	processes;	/* The array of active processes */
static struct process *	clfree;		/* Free process structures */
static struct thread *	threadfree;	/* Free thread structures */
static struct thread *	scavthread;	/* Thread that just died. */
static struct thread *	lastwoken;	/* Last woken thread */

static unsigned 	totrun;		/* Total runnable threads */
#define STARTRUN(t)	\
    do {\
	assert(!(t->mx_flags&TDS_RUN));\
	t->mx_flags|=TDS_RUN;\
	t->mx_process->pr_nrun++;\
	totrun++;\
    } while(0)
#define STOPRUN(t)	\
    do {\
	assert(t->mx_flags&TDS_RUN);\
	assert(t->mx_process->pr_nrun > 0);\
	t->mx_flags &= ~TDS_RUN;\
	t->mx_process->pr_nrun--;\
	totrun--;\
    } while(0)

int			n_aw_timeout;	/* true if awaiting threads */
signum			thread_sig;
long			thread_sigpend;
#if 0
long			thread_nosys;
#endif
struct process *	kernelprocess;	/* The kernel */
struct process *	curusercl;	/* The current user process */

long			loadav;		/* vague indication of system load */
#define DECAY 5		/* loadav decays in 2**DECAY deciseconds */

short schedlevel;


#ifdef STATISTICS
struct mpxstat {
    long s_realswitch;
    long s_fastswitch;
    long s_noswitch;
    long s_uswitch;
    long s_hfree, s_hdup, s_hcoll, s_hhit;
} mpxstat;
#define DO_STATISTICS(incr) do { incr; } while (0)
#else
#define DO_STATISTICS(incr) do {} while (0)
#endif


/*
** An attempt to make inter-process sleep/wakeup faster. We keep a
** hashed cache of sleep calls. In this cache, we keep
** the event and a pointer to the thread sleeping on the cond. var.
** As soon as a second thread sleeps on it, we zap the pointer, and wakeup
** will be it's old slow self. If there's only one thread sleeping on the
** cond. var (as is the common case), however, wakeup can find this thread
** immediately.
*/
#ifdef ROMKERNEL
#define NHASHBIT 3
#else
#define NHASHBIT 9
#endif
#define NHASH (1<<NHASHBIT)
#define EHASH(e) (((long)(e) ^ ((long)(e)>>NHASHBIT)) & (NHASH-1))
struct ehash {
    event	eh_event;
    long	eh_count;
    struct thread	*eh_thread;
} ehash[NHASH];


#ifndef NO_MPX_REGISTER
/* Allow other kernel modules that are interested in thread creation and
 * removal to register upcalls.  Currently used by rpc/group and rawflip
 * modules (if available), so MPX_MAX_UPCALLS is 3.
 */
#define MPX_MAX_UPCALLS	3

static struct {
    void  (*upcall_init)    _ARGS((struct thread *t));
    void  (*upcall_stop)    _ARGS((struct thread *t));
    int   (*upcall_sendsig) _ARGS((struct thread *t));
    void  (*upcall_exit)    _ARGS((struct thread *t));
} mpx_upcalls[MPX_MAX_UPCALLS];

static int mpx_nr_upcalls;

void
mpx_register(init, stop, sendsig, exit)
void  (*init)    _ARGS((struct thread *t));
void  (*stop)    _ARGS((struct thread *t));
int   (*sendsig) _ARGS((struct thread *t));
void  (*exit)    _ARGS((struct thread *t));
{
    if (mpx_nr_upcalls >= MPX_MAX_UPCALLS) {
	panic("mpx: trying to install too many upcalls");
    }
    
    mpx_upcalls[mpx_nr_upcalls].upcall_init = init;
    mpx_upcalls[mpx_nr_upcalls].upcall_stop = stop;
    mpx_upcalls[mpx_nr_upcalls].upcall_sendsig = sendsig;
    mpx_upcalls[mpx_nr_upcalls].upcall_exit = exit;
    mpx_nr_upcalls++;
}

static void
mpx_upcall_init(t)
struct thread *t;
{
    register int i;

    for (i = 0; i < mpx_nr_upcalls; i++) {
	void (*fun)() = mpx_upcalls[i].upcall_init;

	if (fun != 0) {
	    DPRINTF(0, ("mpx_upcall_init: %d: 0x%lx(%d)\n", i, fun,
			THREADSLOT(t)));
	    (*fun)(t);
	}
    }
}

static void
mpx_upcall_stop(t)
struct thread *t;
{
    register int i;

    DPRINTF(0, ("mpx_stop(%d) ", THREADSLOT(t)));
    for (i = 0; i < mpx_nr_upcalls; i++) {
	void (*fun)() = mpx_upcalls[i].upcall_stop;

	if (fun != 0) {
	    DPRINTF(1, ("mpx_upcall_stop: %d: 0x%lx(%d)\n", i, fun,
			THREADSLOT(t)));
	    (*fun)(t);
	}
    }
}

static int
#ifdef __GNUC__
__inline__
#endif
mpx_upcall_sendsig(t)
struct thread *t;
{
    register int i, n_upcalls;

    DPRINTF(0, ("mpx_sig(%d) ", THREADSLOT(t)));
    n_upcalls = mpx_nr_upcalls;
    for (i = 0; i < n_upcalls; i++) {
	int (*fun)() = mpx_upcalls[i].upcall_sendsig;

	if (fun != 0) {
	    DPRINTF(1, ("mpx_upcall_sendsig: %d: 0x%lx(%d)\n", i, fun,
			THREADSLOT(t)));
	    if ((*fun)(t)) {
		DPRINTF(0, ("mpx_upcall_sendsig: done\n"));
		return 1;
	    }
	}
    }

    /* not busy in one of the registered modules */
    return 0;
}

static void
mpx_upcall_exit(t)
struct thread *t;
{
    register int i;

    DPRINTF(0, ("mpx_exit(%d) ", THREADSLOT(t)));
    for (i = 0; i < mpx_nr_upcalls; i++) {
	void (*fun)() = mpx_upcalls[i].upcall_exit;

	if (fun != 0) {
	    DPRINTF(0, ("mpx_upcall_exit: %d: 0x%lx(%d)\n", i, fun,
			THREADSLOT(t)));
	    (*fun)(t);
	}
    }
}

#endif /* NO_MPX_REGISTER */

/*
 * Interrupt handling:
 * The idea is that the kernel runs with interrupts enabled almost all
 * the time, and that interrupts are divided in two parts, the lowlevel
 * routine, which handles the device and all timing critical code,
 * and then enqueues the highlevel routine. The highlevel routine is run
 * as soon as the running kernelthread blocks, and can do linked list
 * manipulation and other dangerous code.
 */

static struct intlist {
	void (*il_rout)();
	long il_param;
} intlist[MAX_INTLIST];
static struct intlist *intfirst;
static struct intlist *intnext;
int n_enq_intr;			/* Accessed from assembler */

#ifdef INTR_OVL_DEBUG
int enqueue_problem;
#endif /* INTR_OVL_DEBUG */

void
initinterrupts() {
    /*
     * Initialize pointers for enqueue structure
     * Must be done before interrupts are enabled
     */

    intfirst = intlist;
    intnext  = intlist;

    enable();
}

void
enqueue(rout, param)
void (*rout)();
long param;
{

#ifdef INTR_OVL_DEBUG	/* Fills up print buffer */
	if (n_enq_intr > MAX_INTLIST / 2)  
	{
#if 0
		enqueue_problem= 1;

		if (n_enq_intr > MAX_INTLIST - 3)
		{
			printf("%s, %d: ", __FILE__, __LINE__);
			stacktrace();
		}
#else
		int i;
		struct intlist * p = intfirst;

		printf("\n%s, %d:\n", __FILE__, __LINE__);
		stacktrace();
		printf("interrupt queue\n");
		for (i = 0; i < n_enq_intr; i++) {
		    printf("%x (%x)  ", p->il_rout, p->il_param);
		    if ((i & 3) == 3) printf("\n");
		    if (++p == &intlist[MAX_INTLIST])
			p = intlist;
		}
		printf("\n");
#endif
	}
	else
		enqueue_problem= 0;
#endif /* INTR_OVL_DEBUG */
	if (n_enq_intr == MAX_INTLIST) {
		/* Maybe an alternative is to not enable interrupts */
		panic("interrupt queue overflow");
		/*NOTREACHED*/
	}
	intnext->il_rout = rout;
	intnext->il_param = param;
	if (++intnext == &intlist[MAX_INTLIST])
		intnext = intlist;
	n_enq_intr++;
	if (schedlevel == PRIO_KERNEL)
		schedlevel++; 		/* make sure the list is checked */
}

void
checkints()
{
	register struct thread *save_curthread = curthread;

	/* Set curthread to a kernel thread while calling the interrupt
	 * handler.  It could be unlocking a mutex!
	 */
	if (kernelprocess != NULL) {
	    /* all kernel threads have priority 0 */
	    curthread = kernelprocess->pr_prioqs[0];
	}
	while (n_enq_intr > 0) {
		(*intfirst->il_rout)(intfirst->il_param);
		disable();
		if (++intfirst == &intlist[MAX_INTLIST])
			intfirst = intlist;
		n_enq_intr--;
		enable();
	}
	curthread = save_curthread;
}


/*
** Scheduler - start next runnable thread.
** If a dead thread is found, it is handed to the scavenger.
*/
void
scheduler() {
    register struct process *cp;
    register struct thread *tp;
    register struct thread *ftp;
    register struct process *oldusercl = curusercl;

    assert(curthread);
    assert(curusercl);
    assert(interrupts_enabled());

    /*
     * At this point note that the variable curusercl is misnamed.
     * It could very well be the kernel.
     */

again:
    if (n_enq_intr > 0) {
	DPRINTF(2, ("XC %d (%d)\n", THREADSLOT(curthread), totrun));
	checkints();
    }
    while (totrun == 0) {
	DPRINTF(2, ("XW %d (%d)\n", THREADSLOT(curthread), totrun));
	while (n_enq_intr <= 0) {
	    waitint();
	}
	DPRINTF(2, ("XC %d (%d)\n", THREADSLOT(curthread), totrun));
	checkints();
    }

    /*
     * If we're the only runnable thread just return.
     */
    if (totrun == 1 && (curthread->mx_flags & TDS_RUN)) {
	schedlevel = PRIO_KERNEL;
	DO_STATISTICS(mpxstat.s_noswitch++);
	assert((curthread->mx_flags & TDS_DEAD) == 0);
	DPRINTF(2, ("XI %d\n", THREADSLOT(curthread)));
#ifdef USER_INTERRUPTS
	/* A device interrupt handler may have signalled the current thread */
	thread_sig = curthread->mx_lastsig;
	thread_sigpend = curthread->mx_flags & SIGFLAGS;
	if (thread_sigpend) {
	    END_MEASURE(sig_call);
	}
#endif
	return;
    }

    BEGIN_MEASURE(mpx_start_scheduler);
#ifndef NOPROC
    if (schedlevel > PRIO_USER) { /* time-slice */
	/*
	 * Scheduling because of timer. Start next
	 * process.
	 */
	DO_STATISTICS(mpxstat.s_uswitch++);
	curusercl = curusercl->pr_nextcl;
    }
#endif
    schedlevel = PRIO_KERNEL;
    /*
     * Shortcut: if there's only one runnable thread which was just
     * woken by wakeup we can immediately take that one.
     */
    if (totrun == 1 && lastwoken && (lastwoken->mx_flags & TDS_RUN)) {
	tp = lastwoken;
	cp = tp->mx_process;
	lastwoken = 0;
	DO_STATISTICS(mpxstat.s_fastswitch++);
	DPRINTF(2, ("XF %d\n", THREADSLOT(tp)));
	goto found;
    }
    lastwoken = 0;

    if (kernelprocess->pr_nrun) {
	/*
	 * If the kernel has runnable threads, it will
	 * be run.
	 */
	cp = kernelprocess;
    }
    else {
	cp = curusercl;
    }
    do {
	assert(cp != NILPROCESS);
	/*
	 * So, see whether this process is runnable.  It should not
	 * be stopped, while it still has some runnable threads in it.
	 */
	if (cp->pr_nrun) {
	    /*
	    ** It has some runnable processes!
	    */
	    if (cp->pr_flags & CL_PREEMPTIVE) {
		/* We do preemptive scheduling between threads */
		int pri;

		pri = NUM_PRIORITIES;
		while (--pri >= 0) {
		    ftp = tp = cp->pr_prioqs[pri];
		    if (tp != 0) {
			assert(tp->mx_priority == pri);
			do
			{
			    tp = tp->mx_nextthread; /* time-slice */
			    assert(tp);
			    assert(tp->mx_priority == pri);
			    if (tp->mx_flags & TDS_RUN) {
#ifdef FAST_SPARC_SWITCH
				if (tp != curthread) {
				    DO_STATISTICS(mpxstat.s_realswitch++);
				} else {
				    DO_STATISTICS(mpxstat.s_noswitch++);
				}
#else
				DO_STATISTICS(mpxstat.s_realswitch++);
#endif
				DPRINTF(2, ("XR %d (%d)\n", THREADSLOT(tp),
					    totrun));
				goto found;
			    }
			} while (tp != ftp);
		    }
		}
	    }
	    else
	    {
		/* We do non-preemptive scheduling - all threads have prio 0 */
		ftp = tp = cp->pr_prioqs[0];
		if (tp == NULL) {
		    DPRINTF(0, ("process %d is ill, flags=%x\n",
				PROCSLOT(cp), cp->pr_flags));
		}
		assert(tp);
		do {
		    if (tp->mx_flags & TDS_RUN) {
#ifdef FAST_SPARC_SWITCH
			if (tp != curthread) {
			    DO_STATISTICS(mpxstat.s_realswitch++);
			} else {
			    DO_STATISTICS(mpxstat.s_noswitch++);
			}
#else
			DO_STATISTICS(mpxstat.s_realswitch++);
#endif
			DPRINTF(2, ("XN %d (%d)\n", THREADSLOT(tp), totrun));
			goto found;
		    }
		    else
			tp = tp->mx_nextthread;
		    assert(tp);
		} while (tp != ftp);
	    }
	}
	/*
	 * Tough luck. Try the next process.
	 */
	cp = cp->pr_nextcl;
	tp = 0;
    } while (cp != curusercl);
found:
    if (tp == 0) {
	/* Should not happen, since totrun is > 0 */
#ifndef SMALL_KERNEL
	mpxdump((char *) 0, (char *) 0);
#endif
	panic("no runnable thread while totrun = %d\n", totrun);
    }
#ifndef NOPROC
    /*
     * Found something. Let's just make sure it isn't dead...
     */
    if (tp->mx_flags & TDS_DEAD) {
	/*
	 * See whether the scavenger is free...
	 */
	if (scavthread == 0) {
	    scavthread = tp;
	}
	else
	{
#ifndef SMALL_KERNEL
	    mpxdump((char *) 0, (char *) 0);
#endif
	    panic("scheduler: scavenger inactive while scavthread != 0\n");
	}
	wakeup((event) &scavthread);
	/* Try to find other thread to schedule */
	goto again;
    }
#endif /* NOPROC */
    assert(tp->mx_flags & TDS_RUN);

#ifdef FAST_SPARC_SWITCH
    /*
     * If the selected thread is still the originally running thread
     * (e.g. because it has the highest priority) we can just return.
     * On the Sparc this avoids a call to fast_flush_windows followed
     * by a number of window underflows.
     */
    if (tp == curthread) {
	return;
    }
#endif

    /*
     * Set current user process (if this *is* a user process).
     */
#ifndef NOPROC
    if (cp != kernelprocess) {
	curusercl = cp;
    }
    if (curusercl != oldusercl && curusercl != kernelprocess) {
	/* change MMU maps */
	usemap((uint16) PROCSLOT(curusercl));
    }
#endif /* NOPROC */
    /*
     * Set current thread pointer, and go!
     * Also, take care that possible signals are handled.
     */
    cp->pr_prioqs[tp->mx_priority] = tp;

#ifndef NOPROC
    /* If the process has told us the address of its _thread_local pointer,
     * update it now.  For safety, we umap() the pointer each time we use it.
     */
    if (cp->pr_local_ptr != 0) {
	phys_bytes local;
        long     **threadlocal;

#ifndef NO_FAST_UMAP
	/* try to use cached version */
	if (cp->pr_map_local_ptr == 0) {
	    cp->pr_map_local_ptr = umap(tp, (vir_bytes) cp->pr_local_ptr,
					(vir_bytes) sizeof(long **), 1);
	}
	local = cp->pr_map_local_ptr;
#else
	local = umap(tp, (vir_bytes) tp->mx_process->pr_local_ptr,
		         (vir_bytes) sizeof(long **), 1);
#endif
	if (local == 0) {
	    printf("pid %d: invalid _thread_local pointer 0x%lx\n",
		   getpid(tp), tp->mx_process->pr_local_ptr);
	    /* kill process? */
	} else {
	    threadlocal = (long **) PTOV(local);
	    *threadlocal = (long *) tp->mx_local;
	}
    }
#endif /* NOPROC */
    thread_sig = tp->mx_lastsig;
    thread_sigpend = tp->mx_flags & SIGFLAGS;
#if 0
    thread_nosys  = tp->mx_flags & TDS_NOSYS;
#endif
#ifdef USER_INTERRUPTS
    if (thread_sigpend) {
	END_MEASURE(sig_call);
    }
#endif
    END_MEASURE(mpx_start_scheduler);
    BEGIN_MEASURE(mpx_end_scheduler);
    switchthread(tp);
    END_MEASURE(mpx_end_scheduler);
}

#ifndef NOPROC
/*
 * You can only change thread priorities if preemptive scheduling
 * has been enabled.  It is too messy otherwise.
 */
int
thread_set_priority(new, old)
long	new;
long *	old;
{
    register struct thread * tp;
    register struct process * pp;

    pp = curthread->mx_process;
    assert(pp != 0);
    if (pp->pr_flags & CL_PREEMPTIVE) {
	if ((*old = curthread->mx_priority) == new) /* No change required */
	    return 0;
	if (new >= NUM_PRIORITIES) {
	    progerror();
	    return -1;
	}
	curthread->mx_priority = new;

	/* Now we have to change the queue our thread is in */

	/* Delete from old priority ring */
	tp = curthread;
	if (tp->mx_nextthread == curthread) /* only one entry */
	    pp->pr_prioqs[*old] = 0;
	else {
	    /* Find entry immediately before current */
	    while (tp->mx_nextthread != curthread)
		tp = tp->mx_nextthread;
	    tp->mx_nextthread = curthread->mx_nextthread;
	    if (pp->pr_prioqs[*old] == curthread)
		pp->pr_prioqs[*old] = curthread->mx_nextthread;
	}

	/* Now insert into new priority ring */
	if (pp->pr_prioqs[new]) {
	    curthread->mx_nextthread = pp->pr_prioqs[new]->mx_nextthread;
	    pp->pr_prioqs[new]->mx_nextthread = curthread;
	} else {
	    /* A ring of 1 */
	    curthread->mx_nextthread = curthread;
	    pp->pr_prioqs[new] = curthread;
	}
	scheduler();
	return 0;
    }
    progerror();
    return -1;
}

int
thread_get_max_priority(max)
long * max;
{
    *max = NUM_PRIORITIES - 1;
    return 0;
}

int
thread_enable_preemption(ptr)
long **ptr;
{
    assert(curthread->mx_process != kernelprocess);
    if (sys_setlocal(ptr) != 0) {
	progerror();
	return -1;
    }
    curthread->mx_process->pr_flags |= CL_PREEMPTIVE;
    return 0;
}
#endif /* NORPOC */


#ifndef NDEBUG
checkstack()
{
  register cursp;

  if (curthread == 0)	/* It is very early in the kernel's life */
	return;
  cursp = curthread->mx_sp;
  if (cursp == 0)
	return;
  compare(cursp, >=, curthread->mx_stbot);
  compare(cursp, <=, curthread->mx_sttop);
}
#endif /* NDEBUG */


threadswitch()
{
     if (totrun > 1 && !(curthread->mx_process->pr_flags & CL_PREEMPTIVE)) {
	 curthread->mx_process->pr_prioqs[curthread->mx_priority] =
	     curthread->mx_nextthread;
     }
     scheduler();
}

/*
** progerror - Programming error found.
*/
void
progerror()
{
    if (!userthread())
	panic("kernel programming error");
    putsig(curthread, (signum) EXC_SYS);
}

/*
** await - await event or timeout.
*/

interval
await(ev, tout)
register event	ev;
interval	tout;
{
    register struct ehash *eh;
    register struct thread *t=curthread;

    assert(ev != (event) 0 || tout != 0);
    assert(t != NILTHREAD);

    STOPRUN(t);
    t->mx_event = ev;
    t->mx_timeout = tout;
    if (tout > 0) n_aw_timeout++;
    /*
    ** Make an entry in the hash table, if possible.
    */
    eh = ehash + EHASH(ev);
    if (eh->eh_count == 0) {
	/*
	** It's free. Fill it in.
	*/
	eh->eh_event = ev;
	eh->eh_thread = t;
	DO_STATISTICS(mpxstat.s_hfree++);
    } else
    if (eh->eh_event == ev) {
	/*
	** There's already somebody else waiting on this ev.
	*/
	eh->eh_thread = 0;
	DO_STATISTICS(mpxstat.s_hdup++);
    }
    else
	DO_STATISTICS(mpxstat.s_hcoll++);

    /*
    ** Else, the hashtable entry is in use for another event.
    ** Too bad.
    */
    eh->eh_count++;

    assert(!(t->mx_flags & TDS_INT));

    scheduler();

    assert(t->mx_flags & TDS_RUN);
    if (t->mx_flags & (TDS_INT|TDS_LSIG|TDS_HSIG)) {
	t->mx_flags &= ~TDS_INT;
	return(-1);
    } else
	return(0); /* BUG-should return timeout */
}

#ifdef AWAIT_DEBUG

interval
await_reason(ev, tout, reason)
event	 ev;
interval tout;
char    *reason;
{
    interval retval;

    curthread->tk_mpx.MX_awaitreason = reason;
    if ((retval = await(ev, tout)) != 0) {
	/* printf("await (%s) failed\n", reason); */
    }
    curthread->tk_mpx.MX_prevreason = reason;
    curthread->tk_mpx.MX_awaitreason = NULL;
    return retval;
}

#endif

/*
** wakeup - resume processes awaiting (sleeping).
*/
void
wakeup(ev)
register event ev;
{
    register struct thread *t;
    register struct ehash *eh;

    assert (ev != (event)0);
    eh = ehash + EHASH(ev);
    if (eh->eh_event == ev && eh->eh_thread) {
	DO_STATISTICS(mpxstat.s_hhit++);
	eh->eh_count--;
	eh->eh_event = 0;
	lastwoken = t = eh->eh_thread;
	t->mx_event = (event)0;
	t->mx_timeout = 0;
	STARTRUN(t);
	schedlevel = PRIO_USER+1;
	return;
    }
    for (t=thread; t<upperthread; t++) {
	if (t->mx_event == ev) {
	    eh->eh_count--;
	    lastwoken = t;
	    t->mx_event = (event)0;
	    t->mx_timeout = 0;
	    STARTRUN(t);
	    schedlevel = PRIO_USER+1;
	}
    }
}

/*
** Wakethread - set a thread runnable.
*/
wakethread(t)
register struct thread *t;
{
    register struct ehash *eh;

    if (! (t->mx_flags & TDS_RUN))
    {
	eh = ehash + EHASH(t->mx_event);
	eh->eh_count--;
	if (eh->eh_event == t->mx_event)
	    eh->eh_thread = 0;
	t->mx_event = (event) 0;
	t->mx_timeout = 0;
	lastwoken = t;
	STARTRUN(t);
    }
}


/*
** Threadevent - return event on which thread is blocked. If the thread
** is not blocked, return 0.
*/
event mpx_threadevent(t)
register struct thread *t;
{
    return(t->mx_event);
}

#ifndef NO_MPX_REGISTER
int
mpx_nthread(t)
register struct thread *t;
{
    return t->mx_process->pr_nthread;
}
#endif

/*
** mpxsweep -	gets called by clock_int()
**		Decrements timeouts for "await" calls.  It wakes up anyone
**		who has timed out.
*/
/*ARGSUSED*/
void mpxsweep(arg)
long arg;
{
    register struct thread *t;

    loadav = ( (((long) totrun)<<16) + (loadav << DECAY) - loadav) >>DECAY;
    if (n_aw_timeout == 0)
	return;		/* nothing else to do in that case */
    n_aw_timeout = 0;
    for (t = thread; t < upperthread; t++) {
	if (!(t->mx_flags & TDS_RUN) && t->mx_timeout > 0) {
	    t->mx_timeout -= RESSWEEP;
	    if (t->mx_timeout <= 0) {
		t->mx_flags |= TDS_INT;
		wakethread(t);
	    } else
		n_aw_timeout++;
	}
    }
}


/*
 * Mutex support routines.
 */

#define GET_MUTEX(s)	(*(s))
#define SET_MUTEX(s,v)	{ *(s) = v; }

static struct mu_ref *muref_freelist;

static struct mu_ref *
mu_find(cl, mu)
register struct process *cl;
register mutex *mu;
{
    /* Return the mutex reference of process cl that is points on mu.
     * If not found, return NULL.  The threads blocked on the mutex can
     * be found using the reference and by following the mu_next list.
     */
    register struct mu_ref *mr;

    for (mr = cl->pr_muref; mr != NULL; mr = mr->mr_next) {
	if (mr->mr_mutex == mu) {
	    assert(mr->mr_first != NULL);
	    return mr;
	}
    }

    return NULL;
}

static void
mu_add(t, mu)
struct thread *t;
mutex *mu;
{
    register struct mu_ref *mr;

    assert(t->mx_muref == NULL);
    /* Add thread t to the list of threads blocked on mutex mu */
    mr = mu_find(t->mx_process, mu);
    if (mr == NULL) {
	DPRINTF(2, ("mu_add: %d sleeps on new mutex 0x%lx\n",
		    THREADSLOT(t), mu));
	/* We are the first to go to sleep on this mutex.
	 * Get a new mutex reference.
	 */
	if (muref_freelist == NULL) {	/* should not happen */
	    panic("out of mutex references");
	}
	mr = muref_freelist;
	muref_freelist = mr->mr_next;

	mr->mr_mutex = mu;
	mr->mr_first = mr->mr_last = t;
	mr->mr_next = t->mx_process->pr_muref;
	t->mx_process->pr_muref = mr;
	t->mx_muref = mr;
    } else {
	assert(mr->mr_first != NULL);
	assert(mr->mr_last != NULL);
	assert(mr->mr_first->mx_muref != NULL);

	/* Set the mutex reference and insert it in the waiting list */
	t->mx_muref = mr;
	if (t->mx_process->pr_flags & CL_PREEMPTIVE) {
	    /*
	     * We want the thread with the highest priority to be woken up first
	     * but between threads of equal priority we use FIFO for fairness
	     */
	    struct thread **prevp, *next;

	    /* Find where t belongs in the list */
	    for (prevp = &mr->mr_first, next = mr->mr_first;
		 next != NULL;
		 prevp = &next->mx_munext, next = next->mx_munext)
	    {
		if (next->mx_priority < t->mx_priority) {
		    /* have to put it before next */
		    DPRINTF(2, ("mu_add: %d is put before %d (mutex 0x%lx)\n",
				THREADSLOT(t), THREADSLOT(next), mu));
		    break;
		}
	    }

	    *prevp = t;
	    t->mx_munext = next;
	    if (next == NULL) {	/* was added to the tail */
		DPRINTF(2, ("mu_add: %d is on tail of list for mutex 0x%lx\n",
			    THREADSLOT(t), mu));
		mr->mr_last = t;
	    }
	}
	else
	{
	    /* All threads have equal priority - use FIFO to provide fairness */
	    assert(mr->mr_last->mx_munext == NULL);
	    t->mx_munext = NULL;
	    mr->mr_last->mx_munext = t;
	    mr->mr_last = t;
	    DPRINTF(2, ("mu_add: %d is on tail of list for mutex 0x%lx\n",
			THREADSLOT(t), mu));
	}
    }
}

static void
mu_remove(t)
register struct thread *t;
{
    /* Remove thread t from the list of threads blocked on the same mutex */
    register struct mu_ref *mr;
    register struct thread *prev, *cur;

    DPRINTF(2, ("mu_remove: %d\n", THREADSLOT(t)));
    mr = t->mx_muref;
    assert(mr != NULL);

    for (prev = NULL, cur = mr->mr_first;
	 cur != t;
	 prev = cur, cur = cur->mx_munext)
    {
	/* t should be on the list */
	assert(cur != NULL);
    }

    if (prev == NULL) {		/* removed first one */
	mr->mr_first = t->mx_munext;
    } else {
	prev->mx_munext = t->mx_munext;
    }

    if (t->mx_munext == NULL) {	/* removed last one */
	mr->mr_last = prev;
	if (prev == NULL) {
	    /* No more threads waiting for this mutex. */
	    register struct mu_ref **mr_prevp;

	    DPRINTF(2, ("mu_remove: free mu_ref 0x%lx\n", mr->mr_mutex));

	    /* Remove the mutex reference from the process. */
	    for (mr_prevp = &t->mx_process->pr_muref;
		 *mr_prevp != mr;
		 mr_prevp = &(*mr_prevp)->mr_next)
	    {
		assert(*mr_prevp != NULL);
	    }
	    *mr_prevp = mr->mr_next;

	    mr->mr_mutex = NULL;
	    mr->mr_next = muref_freelist;
	    muref_freelist = mr;
	}
    }

    t->mx_munext = NULL;
    t->mx_muref = NULL;
}

#define MU_RESERVE_BYTE(mu)      ((char *) mu)

/*
 * mu_trylock - try to acquire a mutex.  If the mutex contains a 0 it is free.
 *		If at least on bit of it is set, it is locked.
 *		Mutexes are "fair"; ie, the first to go to sleep is the first
 *		to be awoken.
 */
int
mu_trylock(s, tout)
    mutex *s;
    interval tout;	/* timeout in milliseconds */
{
    long val;
    register struct thread *t;

    t = curthread;

    /* If we were just signalled, return immediately */
    if (t->mx_flags & TDS_HSIG) {
	DPRINTF(0, ("mu_trylock: %d signalled\n", THREADSLOT(curthread)));
#ifdef KEES_DEBUG
	{
	   static struct thread *prev;
	   static int count;

	   DPRINTF(0, ("psr: 0x%lx\n", sparc_getpsr()));
	
	   if (t == prev) {
		count++;
		if (count > 10) {
		    /* mpxdump((char *)0, (char *)0); */
		    panic("infinite loop?");
		}
	   } else {
		prev = t;
		count = 0;
	   }
	}
#endif
	return -1;
    }

    /* First, check whether the mutex is actually in use. */
    if ((val = GET_MUTEX(s)) == 0) {
	/* set all bits to one, to indicate that it is locked */
	SET_MUTEX(s, 0xffffffff);
	DPRINTF(1, ("mu_trylock: %d: set mutex 0x%x to -1\n",
		    THREADSLOT(curthread), s));
	return 0;
    }

    if (val == 0xffffffff) {
	/* Upward compatibility: old binaries may check for -1 (or negative).
	 * Set it to something different, so that the thread who got the
	 * lock (we don't know who that is) goes to the kernel when it
	 * unlocks it.
	 */
	SET_MUTEX(s, 0x7fffffff);
	DPRINTF(1, ("mu_trylock: %d: set mutex 0x%x to 0x7fffffff\n",
		    THREADSLOT(curthread), s));
    } else {
	/* other nonzero value; this is architecture dependent */
	DPRINTF(1, ("mu_trylock: %d: mutex 0x%x was 0x%x\n",
		    THREADSLOT(curthread), s, GET_MUTEX(s)));
    }

    /* Now, fast-return if it is a conditional acquire. */
    if (tout == 0) {
	DPRINTF(1, ("mu_trylock: %d: fast failure\n", THREADSLOT(t)));
	return -1;
    }

    /* Set a bit in the mutex so that mu_unlock goes to the kernel */
    *MU_RESERVE_BYTE(s) = 1;

    /* Now, the real thing. Put ourselves on the queue and go to sleep. */
    mu_add(t, s);
    STOPRUN(t);
    if (tout > 0) {
        t->mx_timeout = tout;
	n_aw_timeout++;
    }
    END_MEASURE(mu_trylock);
    scheduler();
    BEGIN_MEASURE(mu_gotlock);

    /* Running again.  Either the timer ran out, or we now have the mutex. */
    assert(t->mx_flags & TDS_RUN);
    if (t->mx_muref != NULL) {
	/* Timed out, so we didn't get the mutex.
	 * We still need to get ourselves off the waiting mutex list.
	 */
	DPRINTF(1, ("mu_trylock: %d: timed out on mutex 0x%x, value 0x%x\n",
		    THREADSLOT(t), s, GET_MUTEX(s)));
	mu_remove(t);

	t->mx_flags &= ~(TDS_LSIG | TDS_INT);
	return -1;
    } else {
	/* We got the mutex; return succes. */
	DPRINTF(1, ("mu_trylock: %d: got mutex 0x%x, contents now 0x%x\n",
		    THREADSLOT(t), s, GET_MUTEX(s)));
	return 0;
    }
}

void
mu_unlock(s)
    mutex *s;
{
    register struct thread *t;
    register struct mu_ref *mr;

    if (GET_MUTEX(s) == 0) {
	DPRINTF(0, ("mu_unlock: unlocking already unlocked mutex"));
	progerror();
	return;
    }

    assert(curthread != NULL);
    mr = mu_find(curthread->mx_process, s);
    if (mr == NULL) {
	/* No other thread is waiting on this mutex; clear it */
	SET_MUTEX(s, 0);
	DPRINTF(1, ("mu_unlock: %d: no one waiting for 0x%x\n",
		    THREADSLOT(curthread), s));
    } else {
	/* Give the mutex over to the first one waiting for it. */
	t = mr->mr_first;
	assert(t->mx_muref != NULL);
	mu_remove(t);
	if (t->mx_flags & TDS_RUN) {
	    /* In this case it was woken up by the sweeper because its timer
	     * ran out.  But now it appears it can get the lock after all,
	     * so we must remove the TDS_INT flag.  We might also have had
	     * a signal.
	     */
	    DPRINTF(1, ("mu_unlock: %d: could get 0x%x after all\n",
			THREADSLOT(t), s));
	    assert(t->mx_flags & (TDS_INT|TDS_LSIG|TDS_HSIG));
	    t->mx_flags &= ~TDS_INT;
	} else {
	    DPRINTF(1, ("mu_unlock: %d: give 0x%x to %d\n",
			THREADSLOT(curthread), s, THREADSLOT(t)));
	    t->mx_timeout = 0;
	    lastwoken = t;
	    STARTRUN(t);
	}
    }
    END_MEASURE(mu_unlock);
}


/*
** putsig - send a signal; to a process (and it's servers)
*/
void
putsig(t, sig)
register struct thread *t;
signum sig;
{
#ifndef NOPROC
    sig_vector vec;
#endif

    int (*hnd)();

    DPRINTF(0, ("putsig(%d, %d) ", THREADSLOT(t), sig));

    /*
    ** Killing a dead (or dying) thread is not a good idea.
    */
    if( t->mx_flags & (TDS_DEAD) ) {
	return;
    }
#ifndef NOPROC
    if( t->mx_process == kernelprocess )
#endif
    {
	/*
	** This is a hack, so that kernel servers
	** can still handle signals the old way.
	*/
	hnd = (int (*)())t->mx_sigvec;
	if (hnd == 0)
		return;
	(*hnd)(t->mx_svlen, sig);
	/* rest before return might not be necessary */
#if 0
	t->mx_flags |= TDS_INT;
#endif
#ifndef NO_MPX_REGISTER
	if (!mpx_upcall_sendsig(t))
#else
	if (!(rpc_sendsig(t) || grp_sendsig(t)))
#endif
	    wakethread(t);
	return;
    }
#ifndef NOPROC
    if( sig > 0 && findcatcher(t, sig, &vec) == 0 ) return;
    t->mx_sig = sig;
    t->mx_lastsig = sig;
    if( sig == 0 )
	t->mx_flags |= TDS_HSIG;
    else
	t->mx_flags |= TDS_LSIG|TDS_USIG;
    if (t->mx_process->pr_flags & CL_FASTKILL) {
#ifndef NO_MPX_REGISTER
	mpx_upcall_stop(t);
#else
	rpc_stoprpc(t);
#ifdef FLIPGRP
	grp_stopgrp(t);
#endif
#endif
    }
#ifndef NO_MPX_REGISTER
    if (mpx_upcall_sendsig(t)) {
#else
    if (rpc_sendsig(t) || grp_sendsig(t)) {
#endif
	if( sig != 0 ) {
	    t->mx_flags &= ~(TDS_HSIG|TDS_LSIG);
	}
    } else {
	if( !(t->mx_flags & TDS_STOP) )
	    wakethread(t);
    }
    if( t == curthread ) {
	thread_sig = t->mx_lastsig;
	thread_sigpend = t->mx_flags & SIGFLAGS;
    }
#endif /* NOPROC */
}

#ifdef USER_INTERRUPTS
/*
 * fast_putsig - a very fast version of putsig used for device interrupts
 * It can avoid most of the general checks.
 */
void
fast_putsig(t, sig)
register struct thread *t;
signum sig;
{
    /* We always add signal state since we don't want to lose the
     * network interrupt just because we happen to be doing an RPC.
     */
    t->mx_lastsig = sig;
    t->mx_flags |= TDS_LSIG|TDS_USIG;
    if (!mpx_upcall_sendsig(t)) {
	if (!(t->mx_flags & TDS_STOP)) {
	    wakethread(t);
	}
    }
}
#endif


/*
** getsig - Set signal handling routine.
*/
void getsig(vec, id)
void (*vec)();
int id;
{
    register struct thread *t = curthread;

    assert( t->mx_process == kernelprocess );
    t->mx_svlen = (long) id;
    t->mx_sigvec= (long) vec;
}

/*
** NewProcess - create a new process.
**
*/
struct process *NewProcess()
{
    register struct process *cl;

    if ((cl = clfree) != NILPROCESS)	/* get free process struct */
    {
	clfree = cl->pr_nextcl;
	(void) memset((_VOIDSTAR) cl, 0, sizeof (*cl));
	/* This defaults to no preemptive scheduling! */
	cl->pr_slotno = cl - processarray;
	if (processes == NILPROCESS)
	{
	/* no active processes yet - start list of active processes */
	    processes = cl;
	    cl->pr_nextcl = cl;
	}
	else
	{
	/* put new process in active list */
	    cl->pr_nextcl = processes->pr_nextcl;
	    processes->pr_nextcl = cl;
	}
    }
    return cl;
}

#ifndef NOPROC
/*
** DelProcess - get rid of a process and everything in it.
*/
DelProcess(cl)
  register struct process *cl;
{
    void map_delmap();

    register struct process *cl2;

    assert(cl->pr_nthread == 0);
    /*
    ** Unmap segments.
    */
    map_delmap(cl);
    /*
    ** Remove process from list.
    */
    cl2 = cl->pr_nextcl;
    assert( cl2 != cl );
    while( cl2->pr_nextcl != cl )
	cl2 = cl2->pr_nextcl;
    cl2->pr_nextcl = cl->pr_nextcl;
    /*
    ** And put into free list.
    */
    (void) memset((_VOIDSTAR) &cl->pr_random, 0, sizeof(port));
    cl->pr_nextcl = clfree;
    clfree = cl;

    if( cl == curusercl )
	curusercl = kernelprocess;
}

/*
** exitprocess - A process dies.
*/
void exitprocess(status)
long status;
{
    void	sendall();

    register struct process *cp = curthread->mx_process;

    /*
    ** Set exit status.
    */
    cp->pr_exitst = status;
    /*
    ** Use fastkill to make sure that any hanging getreqs or trans's die
    */
    cp->pr_flags |= CL_DYING | CL_FASTKILL;
    /*
    ** Kill everyone (including self)
    */
    sendall(cp, 0L, 1);
    /*
    ** Return. We'll be caught before returning to user space.
    */
}
#endif


/*
** NewThreadStruct - create new thread structure.
**
*/
static struct thread *
NewThreadStruct(cl, stack, stksiz)
register struct process *cl;
vir_bytes stack;
vir_bytes stksiz;
{
    register struct thread *t;

    /*
    ** Grab a free thread structure.
    */
    if (threadfree == NILTHREAD) {
	DPRINTF(0, ("NewThreadStruct: out of thread structures\n"));
	return NILTHREAD;
    }
    t = threadfree;
    threadfree = threadfree->mx_nextthread;
    if( t >= upperthread )
	upperthread = t+1;
    t->mx_event = (event)t;
    ehash[EHASH(t)].eh_count++;
    t->mx_flags = 0;
    t->mx_sigvec = 0;
    t->mx_svlen = 0;
    t->mx_sig = 0;
    t->mx_lastsig = 0;
    t->mx_local = 0;
    t->mx_munext = 0;
    t->mx_muref = 0;
    t->mx_stbot = stack;
    t->mx_sp = stack+stksiz;
#ifndef NDEBUG
    t->mx_sttop = t->mx_sp;
#endif /* NDEBUG */
#if KERNEL_GLOCALS
    t->mx_glocal.gtable_size = 0;
#endif
    if (cl->pr_flags & CL_PREEMPTIVE)
	t->mx_priority = curthread->mx_priority;
    else
	t->mx_priority = 0;
    assert(t->mx_priority >= 0);
    assert(t->mx_priority < NUM_PRIORITIES);
    /*
    ** Hang into it's process. If it's the first thread, build
    ** circular list, else hang into circular list.
    */
    t->mx_process = cl;
    /* New threads are created with priority 0 */
    if (cl->pr_prioqs[t->mx_priority] == NILTHREAD) {
	cl->pr_prioqs[t->mx_priority] = t;
	t->mx_nextthread = t;
    } else {
	t->mx_nextthread = cl->pr_prioqs[t->mx_priority]->mx_nextthread;
	cl->pr_prioqs[t->mx_priority]->mx_nextthread = t;
    }
    cl->pr_nthread++;
    return t;
}


/*
** NewThread - create a new thread.
**		NB: can also create new kernel threads on the fly.
*/
struct thread *
NewThread(cl, fp, fa, stksiz)
register struct process *cl;
void (*fp)();
vir_bytes fa;
vir_bytes stksiz;
/*
 * Warning: forkthread is called; the number of arguments of the function
 * calling forkthread is not unlimited. Current wisdom says 6 is max, but
 * check it anyhow before you change it.
 */
{
    register struct thread *t;
    vir_bytes stack;
    
    if( cl == NILPROCESS )
	cl = curusercl;
    assert(stksiz != (vir_bytes)0);
    assert(fp != 0);

    /*
    ** Allocate a stack.
    */
    if( (stack = KSTK_ALLOC(stksiz)) == (vir_bytes) 0) {
	DPRINTF(0, ("NewThread: no space for kernel stack\n"));
	return(NILTHREAD);
    }

    /*
    ** Disable interrupts, and fill it in.
    */
    t = NewThreadStruct(cl, stack, stksiz);

    /*
    ** And start it rolling.
    */
    if( t == NILTHREAD) {
	KSTK_FREE(stack);
	return(NILTHREAD);
    }

    wakethread(t);
#ifdef MIPS
    threadfork_mips(t, fp, fa);
#else
    if( forkthread(t) ) {
#ifndef NO_MPX_REGISTER
	mpx_upcall_init(t);
#else
	rpc_initstate(t);
#endif
	(*fp)(fa);
	assert(0);
    }
#endif
    return(t);
}

#ifdef MIPS
/* cthread_start, called from threadfork_mips */
cthread_start(fp, fa)
	void (*fp)();
	vir_bytes fa;
{
#ifndef NO_MPX_REGISTER
	mpx_upcall_init(t);
#else
	rpc_initstate(t);
#endif
	(*fp)(fa);
	assert(0);
}
#endif

void
NewKernelThread(fp, stksiz)
void (*fp)();
vir_bytes stksiz;
{
    register struct thread *t;

    if (stksiz == 0) {
	stksiz = KSTK_DEFAULT_SIZE;
    }
    t = NewThread(kernelprocess, fp, 0L, stksiz);
    assert(t!=0);
}

#ifndef NOPROC
/*
** exitthread - A thread is dying.
*/
void
exitthread(ready)
long *ready;
{
    register struct thread *tk = curthread;
    register struct process *cl = tk->mx_process;
    void warnhand();
#if KERNEL_GLOCALS
    int i;
#endif

    if (++cl->pr_ndying >= cl->pr_nthread)
	warnhand(cl);
    tk->mx_flags |= TDS_DEAD;
    wakethread(tk);
#if KERNEL_GLOCALS
    for (i = 0; i < tk->mx_glocal.gtable_size; i++)
    {
	if (tk->mx_glocal.gtable[i])
		free(tk->mx_glocal.gtable[i]);
    }
    free(tk->mx_glocal.gtable);
#endif
    if (tk->mx_muref != NULL) {	/* remove the mutex reference */
	mu_remove(tk);
    }

    /* Set the user level flag which will tell the process that this thread
     * has gone.  Useful for freeing user level resources such as stack space.
     */
    if (ready != NULL && *ready == 0) {
	*ready = 1;
    }

    assert(tk == curthread);	/* Suspicious because of history */
    /*
    ** Better call the scheduler to bury us...
    ** We also wakeup the scavthread (so at least *someone* is
    ** runnable)
    */
    if (scavthread == 0)
	scavthread = curthread;
    wakeup((event) &scavthread);
    scheduler();
    /* we should never get here */
    assert(0);
}
#endif

/*
** scavenger - kill dead processes.
*/
scavenger() {
    register struct thread *tk,*t2;
    register struct process *cl;
    int pri;
    void ppro_delobj();

    while(1) {
	scavthread = 0;
	(void) await_reason((event) &scavthread, 0L, "scavenger");
#ifndef NOPROC
	tk = scavthread;
	cl = tk->mx_process;
	DPRINTF(1, ("process %d, thread %d going away\n",
		    PROCSLOT(cl), THREADSLOT(tk)));
	if (tk->mx_flags & TDS_RUN) {
		STOPRUN(tk);
		if (tk == lastwoken) lastwoken = 0;
	}
	cl->pr_ndying--;
#ifndef NO_MPX_REGISTER
	mpx_upcall_exit(tk);
#else
	/* Kill transactions: */
	rpc_cleanstate(tk);
#ifdef FLIPGRP
	if(cl->pr_nthread == 1) grp_destroy(tk);
#endif
#endif
	/* Throw away stack and co-processor state: */
	KSTK_FREE(tk->mx_stbot);
	tk->mx_stbot = 0;
#if defined(i80386)
	ndp_cleanup(tk);
#endif
	/*
	** Unlink from process, if needed.
	*/
	if (--cl->pr_nthread != 0) {
	    t2 = tk;
	    /*
	    ** Find predecessor.
	    */
	    while (t2->mx_nextthread != tk)
		t2 = t2->mx_nextthread;
	    /*
	    ** Unlink.
	    */
	    t2->mx_nextthread = tk->mx_nextthread;
	    /*
	    ** And update first thread, if needed.
	    */
	    pri = tk->mx_priority;
	    DPRINTF(1, ("it had priority %d, event = %x\n", pri, tk->mx_event));
	    if (tk == t2) {
		/* a ring of 1 */
		assert(cl->pr_prioqs[pri] == tk);
		assert(tk == tk->mx_nextthread);
		cl->pr_prioqs[pri] = 0;
	    } else {
		if (cl->pr_prioqs[pri] == tk) {
		    cl->pr_prioqs[pri] = t2;
		}
	    }
	} else {
	    /*
	    ** Hah, it's simple. This is the last thread, so
	    ** we can also throw the process away.
	    */
#ifdef ACTMSG
	    am_cleanstate(PROCSLOT(cl));
#endif
	    /*
	    ** First, delete the object from the ps directory.
	    */
	    ppro_delobj(cl);
	    DelProcess(cl);
	}
	/*
	** Now, free thread structure.
	*/
	tk->mx_nextthread = threadfree;
	threadfree = tk;
	if (tk + 1 == upperthread) {
	    /* highest allocated thread, we can reduce search range now */
	    do
		upperthread--;
	    while (upperthread[-1].mx_stbot == 0);
	    compare(upperthread, >, thread);
	}
#endif
    }
}

/*
** initialize stuff.
*/

void
init_proc_list() {
    register struct process *	cl;

    /*
    ** Initialize list of free processes.
    */
    assert(nproc >= 1);	/* must have one process: the kernel process! */
    processarray = (struct process *)
	aalloc((vir_bytes) (nproc * sizeof (struct process)), 0);
    assert(processarray != NILPROCESS);
    for(cl = processarray+nproc-1; cl >= processarray; cl--) {
	cl->pr_nextcl = clfree;
	clfree = cl;
    }
}

static void
init_mutex()
{
    struct mu_ref *mr;
    int max_murefs;

    /* The number of mutex references is never higher than the maximum
     * number of threads.
     */
    max_murefs = totthread;
    muref_freelist = (struct mu_ref *)
	aalloc((vir_bytes) (max_murefs * sizeof(struct mu_ref)), 0);
    assert(muref_freelist != NULL);

    /* link them all together */
    for (mr = &muref_freelist[0]; mr < &muref_freelist[max_murefs - 1]; mr++) {
	mr->mr_next = mr + 1;
    }
    mr->mr_next = NULL;
}

void
init_thread_list() {
    register struct thread *	t;
    register			i;

    thread = (struct thread *) aalloc((vir_bytes) (totthread * sizeof (struct thread)), 0);
    assert(thread != NILTHREAD);
    threadfree = thread + 1;
    for (t = threadfree, i = 1; t < thread + totthread - 1; t++, i++) {
	t->mx_nextthread = t + 1;
	t->tk_slotno = i;
    }
    t->mx_nextthread = NILTHREAD;
    t->tk_slotno = i;

    upperthread = threadfree;	/* To reduce search range */
}

void
init_first_thread() {
    register struct process *	cl;
    register struct thread *	t;
#ifndef NDEBUG
    extern char			kst_beg;	/* Kernel stack limits */
    extern char			kst_end;
#endif /* NDEBUG */
    /*
    ** We create a kernel process, and become a
    ** thread ourselves.
    */
    curusercl = kernelprocess = cl = NewProcess();
    assert(cl != NILPROCESS);
    t = NewThreadStruct(cl, (vir_bytes) 0, (vir_bytes) 0);
    assert(t != NILTHREAD);
#ifndef NDEBUG
    t->mx_sttop = (vir_bytes) &kst_end;
    t->mx_stbot = (vir_bytes) &kst_beg;
#endif /* NDEBUG */
    curthread = t;
    wakethread(t);

    /* Now that curthread is valid, we can start using mutexes. */
    init_mutex();
}

#ifndef NOPROC

#ifndef TIMESLICE
#define TIMESLICE 200L		/* A bit optimistic, current code is wrong */
#endif

/*ARGSUSED*/
static void
next_timeslice(arg)
long arg;
{

	schedlevel = PRIO_USER+1;
}
#endif

void
init_mpx_sweep() {
	void sweeper_set();

	sweeper_set(mpxsweep, 0L, RESSWEEP, 0);
#ifndef NOPROC
	sweeper_set(next_timeslice, 0L, TIMESLICE, 0);
#endif
}

#ifndef SMALL_KERNEL
mpxdump(begin, end)
char *	begin;	/* start of buffer in which to dump info */
char *	end;	/* end of buffer */
{
    register struct thread *	t;
    register struct process *	c;
    register uint16		nt;
    register char *		p;
    int				i;

    p = bprintf(begin, end,
"P# #T #R T#%s   Event    Mutex Next Timout   Stkbot%s Flags\n",
		" Pr",
#ifndef NDEBUG
		"   Stktop"
#else
		""
#endif
								);

    c = processes;
    do
    {
	assert(c != NILPROCESS);
	p = bprintf(p, end, "%2d %2d %2d   %30s%26x",
	    PROCSLOT(c), c->pr_nthread, c->pr_nrun, c->pr_comment, c->pr_flags);
	if (c->pr_flags & CL_STOPPED)	 p = bprintf(p, end, " Stopped");
	if (c->pr_flags & CL_DYING)	 p = bprintf(p, end, " Dying");
	if (c->pr_flags & CL_WARNED)	 p = bprintf(p, end, " Warned");
	if (c->pr_flags & CL_FASTKILL)	 p = bprintf(p, end, " Fast-kill");
	if (c->pr_flags & CL_PREEMPTIVE) p = bprintf(p, end, " Preemptive");
	p = bprintf(p, end, "\n");
	nt = c->pr_nthread;
	for (i = NUM_PRIORITIES; --i >= 0; )
	{
	    struct thread * ftp;

	    if ((t = c->pr_prioqs[i]) == 0)
		continue;
	    ftp = t;
	    do
	    {
		nt--;
		assert(t != NILTHREAD);
		p = bprintf(p, end, "%11d", THREADSLOT(t));
		p = bprintf(p, end, "%3d", t->mx_priority);
		if (t->mx_event)
		    p = bprintf(p, end, "%9lx", (long)t->mx_event);
		else
		    p = bprintf(p, end, "         ");
		if (t->mx_muref != NULL)
		{
		    p = bprintf(p, end, "%9lx", t->mx_muref->mr_mutex);
		    if (t->mx_munext)
			p = bprintf(p, end, "%5d", THREADSLOT(t->mx_munext));
		    else
			p = bprintf(p, end, "     ");
		}
		else
		    p = bprintf(p, end, "              ");
		if (t->mx_timeout)
		    p = bprintf(p, end, "%7d", t->mx_timeout);
		else
		    p = bprintf(p, end, "       ");
		p = bprintf(p, end, "%9x", t->mx_stbot);
#ifndef NDEBUG
		p = bprintf(p, end, "%9x", t->mx_sttop);
#endif
		p = bprintf(p, end, "%5x", t->mx_flags & 0xFFFF);
		if (t->mx_flags & TDS_RUN)	 p = bprintf(p, end, " run");
		if (t->mx_flags & TDS_EXC)	 p = bprintf(p, end, " exception");
		if (t->mx_flags & TDS_DEAD)	 p = bprintf(p, end, " dead");
		if (t->mx_flags & TDS_INT)	 p = bprintf(p, end, " int");
		if (t->mx_flags & TDS_START) p = bprintf(p, end, " starting");
		if (t->mx_flags & TDS_SRET)	 p = bprintf(p, end, " sigreturn");
		if (t->mx_flags & TDS_LSIG)
		    p = bprintf(p, end, " sig(%ld)", t->mx_sig);
		if (t->mx_flags & TDS_USIG)
		    p = bprintf(p, end, " usig(%ld)", t->mx_lastsig);
		if (t->mx_flags & TDS_HSIG)	 p = bprintf(p, end, " stun");
		if (t->mx_flags & TDS_SHAND) p = bprintf(p, end, " callsig");
		if (t->mx_flags & TDS_STOP)	 p = bprintf(p, end, " stop");
#ifdef AWAIT_DEBUG
		if (t->tk_mpx.MX_awaitreason != NULL) {
		    p = bprintf(p, end, "(%s)", t->tk_mpx.MX_awaitreason);
		}
		if (t->tk_mpx.MX_prevreason != NULL) {
		    p = bprintf(p, end, "(prev %s)", t->tk_mpx.MX_prevreason);
		}
#endif
		p = bprintf(p, end, "\n");

		t = t->mx_nextthread;
	    } while (t != ftp);
	} /* end for */
	assert(nt == 0);
	c = c->pr_nextcl;
    } while (c != processes);
/*
** load average is a fixed point number with decimal point at bit 16.
** when printing it we only show one decimal place which we achieve by
** dividing the load average by 2^16/10.
*/
    p = bprintf(p, end,
      "Current thread is %d, total %u runnable, %d await()ing, loadav %d.%d\n",
		 THREADSLOT(curthread), totrun ,
		 n_aw_timeout, loadav>>16, ((loadav&0xFFFF)/6554));
#ifdef STATISTICS
    p = bprintf(p, end, "User process switches: %d\n", mpxstat.s_uswitch);
    p = bprintf(p, end, "Context switches: %d normal, %d fast, %d immediate\n",
		mpxstat.s_realswitch, mpxstat.s_fastswitch, mpxstat.s_noswitch);
    p = bprintf(p, end,
	    "Event hashing: %d free, %d dups, %d collisions, %d hits\n",
	    mpxstat.s_hfree, mpxstat.s_hdup, mpxstat.s_hcoll, mpxstat.s_hhit);
#endif /* STATISTICS */
    return p - begin;
}
#endif

/*
** getpid - returns process number for a thread
*/
getpid(t)
struct thread *t;
{

	return PROCSLOT(t->mx_process);
}

/*
** userthread - return true if this is a thread in a user process.
*/
userthread() {
    
    return curthread->mx_process != kernelprocess;
}
