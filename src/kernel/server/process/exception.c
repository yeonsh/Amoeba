/*	@(#)exception.c	1.11	96/02/27 14:20:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#define MPX
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "global.h"
#include "exception.h"
#include "kthread.h"
#include "process/proc.h"
#include "procdefs.h"
#include "fault.h"
#include "assert.h"
INIT_ASSERT
#include "seg.h"
#include "map.h"
#include "stdlib.h"
#include "string.h"
#include "sys/proto.h"

#include "debug.h"

extern void		scheduler();

extern signum		thread_sig;
extern long		thread_sigpend;
extern port		segsvr_put_port;

/* How long to locate for an owner */
#define HLOCTIM		((interval) 10000)

/* td_state flags settable by user */
#define UFLAGS		(TDS_FIRST | TDS_NOSYS)

/*
** Part 1: process descriptors.
**
** These routines handle process descriptors. buildpd() makes one
** (either for the getinfo() syscall or for a heavy-weight signal)
** sendpd sends it to the owner, and warnhand() is called on normal
** process termination.
*/

/*
** buildpd - construct a process descriptor.
**	     returns the size of the descriptor!
*/
long
buildpd(cl, cd, len, dothreads, segnames)
struct process *	cl;
process_d *		cd;
int			len;
int			dothreads;
int			segnames;
{
    void		ppro_setprv();

    register struct thread *	tp;
    register thread_d *		td;
    register thread_kstate *	tdk;
    int				maxseg; /* max # seg_d's that fit in buffer */
    int				bytesleft; /* space for thread descriptors */
    int				tdusize;
    int				thissize; /* size of curr. thread descriptor */
    int				pri;
    struct thread *		ftp;

    compare(TDSIZE_MAX, <=, MAX_THREAD_SIZE);
    /*
    ** First, setup per-process state.
    */
    assert(len >= sizeof (process_d));
    assert(cd != (process_d *) 0);
    (void) memset((_VOIDSTAR) cd, 0, (size_t) len);
    (void) strncpy(cd->pd_magic, ARCHITECTURE, ARCHSIZE);
    cd->pd_owner = cl->pr_owner;
    cd->pd_self.cap_port = segsvr_put_port;
    ppro_setprv(cl, &cd->pd_self.cap_priv);
    cd->pd_nthread = 0;

    /*
    ** Put the Segment descriptors into the pd
    ** Don't put more in than will fit in the given buffer.
    */
    maxseg = ((char *) cd + len - (char *) PD_SD(cd)) / sizeof (segment_d);
    assert(maxseg >= 0);
    cd->pd_nseg = getmap(cl, PD_SD(cd), maxseg, !segnames);

    td = PD_TD(cd);	/* find pointer to start of thread descriptors */
    if (!dothreads)
	return (long) ((char *) td - (char *) cd);
    /*
    ** Now, setup per-thread state.
    */
    bytesleft = (char *) cd + len - (char *) td;
    for (pri = 0; pri < NUM_PRIORITIES; pri++)
    {
	if ((ftp = tp = cl->pr_prioqs[pri]) != 0)
	{
	    do
	    {
		assert(tp != 0);
		if (tp->mx_ustate == 0) {
		    printf("buildpd: mx_ustate is NULL\n");
		    break;
		}
		tdusize = USTATE_SIZE((thread_ustate *) tp->mx_ustate);
		thissize = sizeof (thread_d) + sizeof (thread_kstate) + tdusize;
		bytesleft -= thissize;
		if (bytesleft < 0) {
		    printf("buildpd: no room for thread state\n");
		    break;
		}
		td->td_state = tp->mx_flags;
		td->td_len   = thissize;
		td->td_extra = TDX_KSTATE | TDX_USTATE;
		tdk = (thread_kstate *) (td + 1);
		savekstate(tp, tdk);
		(void) memmove((_VOIDSTAR) (tdk+1), (_VOIDSTAR) tp->mx_ustate,
			       (size_t) tdusize);
		td = TD_NEXT(td);
		cd->pd_nthread++;
		tp = tp->mx_nextthread;
	    } while (tp != ftp);
	} /* end if */
    } /* end for */
    return (long) ((char *) td - (char *) cd);
}


/*
** sendpd - Send process descriptor to the owner.  The owner may immediately
**	    send a new pd back to us in the reply.
**
** Returns 1 if a new valid process descriptor was returned from "server".
** Otherwise returns 0.
*/

static int
sendpd(hand, type, sig, pd, len)
capability *	hand;
long		type;
signum		sig;
process_d *	pd;	/* in/out: process descriptor to be sent/received */
long		len;	/* in: size of process descriptor in bytes */
{
    header	hdr;		/* header for trans */
    bufsize	rv;		/* return value of trans */
    bufsize	sent;		/* size of process descriptor sent */
    interval	oloctim;	/* old locate timeout value */
    char *	buf;		/* trans buffer to hold pd */
    char *	p;		/* pointer to end of pd in buffer */

    assert(userthread());
    hdr.h_port = hand->cap_port;
    hdr.h_priv = hand->cap_priv;
    hdr.h_command = PS_CHECKPOINT;
    hdr.h_offset = sig;
    hdr.h_extra = type;
    if ((buf = (char *) malloc((unsigned) len)) == 0)
	return -1;
    if ((p = buf_put_pd(buf, buf + len, pd)) == 0)
    {
	free((_VOIDSTAR)buf);
	return -1;
    }
    if ((p - buf) > MAX_PDSIZE) {
	/* a truncated process descriptor is better than nothing at all.. */
	sent = MAX_PDSIZE;
    } else {
	sent = (bufsize) (p - buf);
    }
    hdr.h_size = sent;
    curthread->mx_flags |= TDS_SHAND;
    oloctim = timeout(HLOCTIM);
    rv = trans(&hdr, (bufptr) buf, sent, &hdr, (bufptr) buf, sent);
    (void) timeout(oloctim);
    curthread->mx_flags &= ~TDS_SHAND;

    if (ERR_STATUS(rv)) {
	DPRINTF(1, ("checkpoint send failed (size %d; sent %d; err: %s)\n",
		    p - buf, sent, err_why((errstat) ERR_CONVERT(rv))));
    } else {
	DPRINTF(1, ("checkpoint sent (size %d; sent %d; rv %d; status %d)\n",
		    p - buf, sent, rv, ERR_CONVERT(hdr.h_status)));
    }
    if (rv == sent && hdr.h_status == STD_OK)	/* they sent back a pd */
    {
	/* The pd they send back is meant to be the same size as the
	** one we sent them so we reuse the buffer pointed to by pd
	*/
	if (buf_get_pd(buf, buf + len, pd) != 0)	/* is it a valid pd? */
	{
	    /* got a good pd so release the buffer and return */
	    free((_VOIDSTAR)buf);
	    return 1;
	}
	printf("sendpd: Illegal process descriptor returned\n");
    }
    free((_VOIDSTAR)buf);
    return 0;
}

/*
** warnhand - Warn owner that the process died.
**		We only send segment descriptors with process_descriptor
*/
void
warnhand(cl)
struct process *	cl;
{
    process_d *	pdbuf;
    unsigned	pdsz;	/* size of buffer for process descriptor */
    long	len;

    if (NULLPORT(&cl->pr_owner.cap_port))
	return;
    if (cl->pr_flags & CL_WARNED)
	return;
    pdsz = sizeof (process_d) + seg_count(cl) * sizeof (segment_d);
    pdbuf = (process_d *) malloc(pdsz);
    cl->pr_flags |= CL_DYING | CL_STOPPED | CL_WARNED;
    len = buildpd(cl, pdbuf, (int) pdsz, 0, 1);
    DPRINTF(3, ("warnhand: sendpd\n"));
    if (sendpd(&cl->pr_owner, (long) TERM_NORMAL, cl->pr_exitst, pdbuf, len) != 0)
	printf("warnhandler: couldn't send process descriptor\n");
    cl->pr_exitst = 0;
    free((_VOIDSTAR) pdbuf);
}

/*
** Part 2: light-weight signal raising.
*/

/*
** sendall - Send signal to all threads in a process.
**
** This routine implements both normal light-weight signals as
** well as heavy-weight ones (by sending uncatchable signal 0 to
** everyone) as well as stopping the other threads after one has gotten
** an uncaught exception (by sending 0 and calling with doself == 0).
*/
void
sendall(cl, signo, doself)
register struct process *cl;
signum	signo;
int	doself;
{
    register struct thread *tp;
    register struct thread *ftp;
    int i;

    if (cl == 0)
	cl = curthread->mx_process;
    for (i = 0; i < NUM_PRIORITIES; i++)
    {
	if ((ftp = tp = cl->pr_prioqs[i]) == 0)
	    continue;
	do
	{
	    assert(tp != 0);
	    if (doself || tp != curthread)
		putsig(tp, signo);
	} while ((tp = tp->mx_nextthread) != ftp);
    }
}

/*
** stopprocess - Put a process in stopped state.
** The 'signo' is the heavy-weight signal to send.
*/
void
stopprocess(cl, signo)
register struct process *cl;
signum			signo;
{
    cl->pr_flags |= CL_STOPPED;
    if ((cl->pr_flags & CL_FASTKILL) == 0)
	cl->pr_nstop = 0;
    if (signo)
    {
	cl->pr_exitst = TERM_STUNNED;
	cl->pr_signal = signo;
    }
    sendall(cl, 0L, signo != 0);
}

/*
** Part 3: light-weight signal catching.
*/

/*
** findcatcher - See whether a catcher routine is available.
*/
int
findcatcher(t, sig, vec)
struct thread *	t;
signum		sig;
sig_vector *	vec;
{
    sig_vector *	cv;	/* catcher vector */
    int			ndone;
    int			retval;

    ndone = t->mx_svlen;
    if (t->mx_sigvec == 0 || ndone == 0 || (t->mx_flags & TDS_SHAND))
	return 0;
    cv = (sig_vector *) PTOV(umap(t, (vir_bytes) t->mx_sigvec,
			    (vir_bytes) ndone * sizeof (sig_vector), 0));
    if (cv == 0)
	return 0;
    retval = 0;
    for (;--ndone >= 0 && cv->sv_type;cv++) {
	if (cv->sv_type == sig)
	{
	    if (cv->sv_pc) {
		*vec = *cv;
		retval = 1;
	    }
	    break;
	}
    }
    return retval;
}

/*
** sigcatch - Set the catch vector.
*/
sigcatch(vec, len)
vir_bytes	vec;
vir_bytes	len;
{
    curthread->mx_sigvec = vec;
    curthread->mx_svlen = len;
}

/*
**
** Part 4: user exception handling.
*/

/*
** usertrap - The user has gotten a trap.
**
** This routine is called from the machine-dependent layer whenever
** a user thread gets a trap. This can either be a real hardware trap,
** or a software exception (set by sendall() or progerror(), for instance)
** Software traps are received later than sent: upon the first kernel
** to user transition (either because a system call finished or because
** a clock (or other) interrupt returned.
**
** Such a software interrupt is also generated upon starting a thread
** which has been migrated to us.
*/
int
usertrap(type, addr)
long		type;
thread_ustate *	addr;
{
    register struct process *cl = curthread->mx_process;
    process_d *		pdbuffer;
    int			buflen;
    long		len;
    struct thread *	tp;
    struct thread *	thread_first;
    process_d *		cd;
    int			i;
    sig_vector		vec;
    thread_d *		td;
    thread_kstate *	tdk;
    thread_ustate *	tdu;
    int			tdusize;
    int			pri;
    struct thread *	ftp;

    /*
    ** Firstly, check whether we're on a sinking ship.
    */
    if (cl->pr_flags & CL_DYING)
    {
	thread_sigpend = 0;
	thread_sig = curthread->mx_sig = 0;
	curthread->mx_flags &= ~(TDS_USIG | TDS_LSIG | TDS_HSIG);
	DPRINTF(1, ("Thread was dying\n"));
	return 0;
    }

    /*
    ** Secondly, check whether it's a starting thread that needs fiddling.
    */
    if (curthread->mx_flags & TDS_START)
    {
	assert(curthread->mx_ustate);
	tdusize = USTATE_SIZE((thread_ustate *) curthread->mx_ustate);
	(void) memmove((_VOIDSTAR) addr, (_VOIDSTAR) curthread->mx_ustate,
		       (size_t) tdusize);
	relblk((vir_bytes) curthread->mx_ustate);
	curthread->mx_ustate = (vir_bytes) 0;
	curthread->mx_flags &= ~TDS_START;
	thread_sigpend &= ~TDS_START;
	DPRINTF(1, ("Thread is starting\n"));
	return 1;
    }

    /*
    ** No, it isn't. Check whether this is a light-weight
    ** signal, and the thread has an owner for it.
    */
    if (thread_sigpend == TDS_USIG)
    {
#ifdef USER_INTERRUPTS
	if (fast_findcatcher(curthread, thread_sig, &vec) ||
	    findcatcher(curthread, thread_sig, &vec))
#else
	if (findcatcher(curthread, thread_sig, &vec))
#endif
	{
	    signum tstemp = thread_sig;

	    DPRINTF(1, ("Found catcher for %d\n", thread_sig));
	    thread_sig = curthread->mx_sig = 0;
	    thread_sigpend = 0;
	    curthread->mx_flags &= ~(TDS_USIG|TDS_LSIG);
	    return callcatcher(tstemp, &vec, addr);
	}
	DPRINTF(1, ("Did not find catcher for %d\n", thread_sig));
	/*
	** There wasn't a signal handler.
	** Convert signal to heavyweight signal.
	*/
    }

try_again:
    DPRINTF(1, ("Thread %d got signal %d pending 0x%x type %d\n",
		THREADSLOT(curthread), thread_sig, thread_sigpend, type));
    if (type < 0)
    {
	/*
	** This is the troublemaker. Flag him so.
	*/
	curthread->mx_flags |= TDS_EXC;
	curthread->mx_process->pr_signal = type;
	curthread->mx_process->pr_exitst = TERM_EXCEPTION;
    }
    thread_sigpend = 0;
    thread_sig = 0;
    curthread->mx_flags &= ~(TDS_LSIG | TDS_USIG | TDS_HSIG);
    curthread->mx_sig = 0;
    cl = curthread->mx_process;
    if (!(cl->pr_flags & CL_STOPPED))
	stopprocess(cl, (signum) 0);
    if (NULLPORT(&cl->pr_owner.cap_port))
	return 0;
    /*
    ** If we're not last, we go to sleep on our address. We'll
    ** get woken up again when the process descriptor is returned.
    */
    curthread->mx_ustate = (vir_bytes) addr;
    if (++cl->pr_nstop < cl->pr_nthread)
    {
	DPRINTF(1, ("thread %d: %d < %d\n",
		    THREADSLOT(curthread), cl->pr_nstop, cl->pr_nthread));
	curthread->mx_flags |= TDS_STOP;
	curthread->mx_flags &= ~TDS_CONTINUE;
	if ((await_reason((event) addr, 0L, "usertrap") < 0) &&
	    ((curthread->mx_flags & TDS_CONTINUE) == 0))
	{
	    /* If the await fails, but the CONTINUE flag is on, then
	     * the process got another signal before this thread was
	     * able to resume after the previous signal was handled.
	     */
	    DPRINTF(1, ("Thread %d: await failed; flags now 0x%x\n",
		        THREADSLOT(curthread), curthread->mx_flags));
	    return 0;
	}
	curthread->mx_flags &= ~(TDS_STOP | TDS_CONTINUE);
	DPRINTF(1, ("Thread %d: await done; flags now 0x%x\n",
		    THREADSLOT(curthread), curthread->mx_flags));
	if (curthread->mx_flags & TDS_DIE) {
	    return 0;
	}
	if (cl->pr_flags & CL_STOPPED) {
	    /* If the CL_STOPPED flag is not cleared, then the process
	     * got another signal before we were able to run.
	     * This can happen when the process is being single stepped.
	     * We just have to retry in this case.
	     */
	    DPRINTF(1, ("Process flags 0x%x; try again\n", cl->pr_flags));
	    goto try_again;
	} else {
	    return 1;
	}
    }

    /*
    ** We're the last. Let's make a process descriptor and call the owner.
    */
    buflen = sizeof (process_d) + cl->pr_nthread * TDSIZE_MAX
					+ GetNmap(cl) * sizeof (segment_d);
    if ((pdbuffer = (process_d *) malloc((unsigned) buflen)) == 0)
    {
	/*
	** No space for cd.
	*/
	printf("usertrap: no space for process descriptor\n");
	goto Bad;
    }
    len = buildpd(cl, pdbuffer, buflen, 1, 1);
    /*
    ** Now, do the transaction.
    */
    DPRINTF(1, ("usertrap calling sendpd (exitstatus = %d, signal = %d)\n",
		cl->pr_exitst, cl->pr_signal));
    if (sendpd(&cl->pr_owner, cl->pr_exitst, cl->pr_signal, pdbuffer, len) <= 0)
    {
	DPRINTF(1, ("usertrap sendpd failed\n"));
Bad:
	/*
	** First, kill all others.
	*/
	assert(curthread->mx_process == cl);
	if (cl->pr_flags & CL_PREEMPTIVE)
	    cl->pr_flags = CL_PREEMPTIVE | CL_DYING | CL_WARNED;
	else
	    cl->pr_flags = CL_DYING | CL_WARNED;
	cl->pr_signal = cl->pr_exitst = 0;
	for (pri = 0; pri < NUM_PRIORITIES; pri++)
	{
	    if ((ftp = tp = cl->pr_prioqs[pri]) != 0)
	    {
		do
		{
		    tp = tp->mx_nextthread;
		    assert(tp);
		    if (tp != curthread)
		    {
			tp->mx_flags |= TDS_DIE;
			wakethread(tp);
		    }
		} while (tp != ftp);
	    }
	}
	if (pdbuffer)
	    free((_VOIDSTAR) pdbuffer);
	return 0;
    }
    DPRINTF(3, ("usertrap sendpd done\n"));
    cl->pr_signal = cl->pr_exitst = 0;
    cl->pr_signal = 0;
    cd = pdbuffer;
    /*
    ** First, some sanity checks...
    */
    if (cd->pd_nthread != cl->pr_nthread)
	goto Bad;
    i = cd->pd_nthread;
    /*
    ** We should check the mapping here. That's a lot of work,
    ** so we'll forget it for the moment.
    */

    /*
    ** Restore thread status.
    */
    td = PD_TD(cd);
    thread_first = curthread;
    DPRINTF(1, ("usertrap: thread_first = %d\n", THREADSLOT(thread_first)));
    for (pri = 0; pri < NUM_PRIORITIES; pri++)
    {
	if ((ftp = tp = cl->pr_prioqs[pri]) != 0)
	{
	    do
	    {
		assert(--i >= 0);
		if (td->td_len < TDSIZE_MIN || td->td_len > TDSIZE_MAX ||
				    td->td_extra != (TDX_KSTATE | TDX_USTATE)) {
		    printf("usertrap: illegal td returned: size %d extra %x\n",
					td->td_len, td->td_extra);
		    goto Bad;
		}
		tdk = (thread_kstate *) (td + 1);
		tdu = (thread_ustate *) (tdk + 1);
		tdusize = USTATE_SIZE(tdu);
		if (td->td_state & TDS_FIRST)
		{
		    /*
		    ** This thread should be started first.
		    */
		    thread_first = tp;
		    td->td_state &= ~TDS_FIRST;
		}
		tp->mx_flags = (tp->mx_flags & ~(UFLAGS | TDS_EXC)) |
						       (td->td_state & UFLAGS);
		if (tp->mx_flags & TDS_STOP) {
		    /* Notify the fact that this thread may continue.
		     * Although we make it runnable below, that's no
		     * guarantee that it will actually run before another
		     * signal comes in.  In that case the await() above will
		     * fail, but that can be ignored then.
		     */
		    tp->mx_flags |= TDS_CONTINUE;
		}
		loadkstate(tp, tdk);
		(void) memmove((_VOIDSTAR) tp->mx_ustate, (_VOIDSTAR) tdu,
			       (size_t) tdusize);
		wakethread(tp);
		td = TD_NEXT(td);
		tp = tp->mx_nextthread;
	    } while (tp != ftp);
	 }
    }
    cl->pr_flags &= ~(CL_STOPPED|CL_DYING|CL_WARNED|CL_FASTKILL);
    free((_VOIDSTAR) pdbuffer);
    if (thread_first != curthread)
    {
	DPRINTF(1, ("usertrap: new thread_first: %d, prio %d\n",
		    THREADSLOT(thread_first), thread_first->mx_priority));
	curthread->mx_process->pr_prioqs[thread_first->mx_priority] =
		thread_first;
	scheduler();
    }
    return 1;
}
