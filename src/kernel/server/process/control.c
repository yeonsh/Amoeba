/*	@(#)control.c	1.10	96/02/27 14:20:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#define MPX
#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "exception.h"
#include "kthread.h"
#include "process/proc.h"
#include "procdefs.h"
#include "cmdreg.h"
#include "stderr.h"
#include "fault.h"
#include "assert.h"
INIT_ASSERT
#include "seg.h"
#include "map.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "module/stdcmd.h"
#include "capset.h"
#include "soap/soap.h"
#include "string.h"
#include "kerneldir.h"
#include "sys/proto.h"
#include "monitor.h"
#include "debug.h"

/* Externals from mpx.c: */
struct process *	NewProcess();
struct thread    *	NewThread();
extern struct process *	processarray;
extern signum		thread_sig;
extern long		thread_sigpend;

extern port		segsvr_put_port;

#ifndef KTHREAD_STKSIZ
/* Amount of kernel stack for a user program: may be changed per architecture */
#define KTHREAD_STKSIZ		4096
#endif


/*-------------------------------------------------------------------
**
** Part 0: the "ps" directory
**
** These routines maintain a SOAP directory containing the capabilities
** of all processes.
** Processes capabilities may be found as <hostcap>/ps/<number>.
**--------------------------------------------------------------------
*/

extern capset _sp_rootdir;
static capset psdir;

/*
** ppro_dirinit -- initialize the ps directory.
*/
void
ppro_dirinit()
{
    /*
    ** The number of strings in colnames must be equal to NPS_COLUMNS
    ** which is defined in procdefs.h.  However the directory that we
    ** are appending it to has NROOTCOLUMNS.
    */
    static char *colnames[] = { "owner", "group", "other", 0 };
    static long cols[NROOTCOLUMNS] =
				{0xFF & ~(SP_DELRGT | SP_MODRGT), 0x2, 0x4};
    
    /*
    ** Create an internal soap directory and append it to the root.
    */
    if (sp_create(&_sp_rootdir, colnames, &psdir) != STD_OK)
	panic("ppro_dirinit: can't create directory");
    if (sp_append(&_sp_rootdir, PROCESS_LIST_NAME,
    				&psdir, NROOTCOLUMNS, cols) != STD_OK)
	panic("ppro_dirinit: can't append directory");
}

/*
** ppro_name -- store process's name in a buffer.
*/
static void
ppro_name(cl, buf, bufend)
struct process *	cl;
char *			buf;
char *			bufend;
{
    if ((buf = bprintf(buf, bufend-1, "%d", (int)PROCSLOT(cl))) == 0)
	panic("ppro_name: bprintf failed"); /* caller's buffer is too narrow */
    *buf = '\0';
}

/*forward*/
void ppro_setprv();

/*
** ppro_addobj -- add capability to proc directory; for create_process.
*/
void
ppro_addobj(cl)
struct process *	cl;
{
    static long cols[SP_MAXCOLUMNS] = { PSR_ALL, 0, 0 };
    char name[20];
    capability cap;
    capset cs;
    errstat err;

    cap.cap_port = segsvr_put_port;
    ppro_setprv(cl, &cap.cap_priv);
    if (!cs_singleton(&cs, &cap))
	panic("ppro_addobj: cs_singleton failed");
    ppro_name(cl, name, name + sizeof name);
    err = sp_append(&psdir, name, &cs, NPS_COLUMNS, cols);
    if (err != STD_OK) {
	printf("ppro_addobj: sp_append failed (%s)\n", err_why(err));
	panic("ppro_addobj: sp_append failed");
    }
    cs_free(&cs);
}

/*
** ppro_delobj -- delete capability from proc directory.
*/
void
ppro_delobj(cl)
struct process *	cl;
{
    char name[20];
    errstat err;

    ppro_name(cl, name, name + sizeof name);
    if ((err = sp_delete(&psdir, name)) != STD_OK) {
	printf("ppro_delobj(%s): sp_delete err = %d\n", name, err);
	panic("ppro_delobj: sp_delete failed"); /* XXX shouldn't, really */
    }
}

/*-------------------------------------------------------------------
**
** Part 1: capability handling routines.
**
** These routines find processes given a capability, create capabilities.
** retract capabilities or restrict rights.
**--------------------------------------------------------------------
*/

/*
** ppro_setprv - Set process capability.
*/
void
ppro_setprv(cl, prv)
struct process *	cl;
private *		prv;
{
    (void) prv_encode(prv, MKPRVNUM(OBJ_P, PROCSLOT(cl)), PSR_ALL,
								&cl->pr_random);
}

/*
** ppro_getprv - Get a process given a capability.
*/
errstat
ppro_getprv(rights, prv, proc)
rights_bits	rights;
private *	prv;
struct process **proc;
{
    objnum		object;
    int			num;
    rights_bits		rrights;
    struct process *	cl;
    extern uint16	nproc;

    object = prv_number(prv);
    if (PRVTYPE(object) != OBJ_P)
	return STD_CAPBAD;
    num = PRVNUM(object);
    if (num < 0 || num >= (int) nproc)
	return STD_CAPBAD;
    cl = processarray + num;
    if (prv_decode(prv, &rrights, &cl->pr_random) != 0)
	return STD_CAPBAD;
    compare(PROCSLOT(cl), ==, num);
    *proc = cl;
    if ((rights & rrights) != rights)
	return STD_DENIED;
    return STD_OK;
}

/*
** ppro_restrict - Remove some rights from a capability.
*/
ppro_restrict(priv, rights)
private *	priv;
rights_bits	rights;
{
    struct process *	cl;
    errstat err;

    if ((err = ppro_getprv(rights, priv, &cl)) != STD_OK)
	return err;
    (void) prv_encode(priv, MKPRVNUM(OBJ_P, PROCSLOT(cl)), rights, &cl->pr_random);
    return 0;
}

/*
** Part 2: thread creation and initialization.
**
** Routines here create new threads and initialize them, in close
** cooperation with stuff in mpx.c
*/

/*
** loadkstate - Load kernel state from thread descr.
*/
loadkstate(tp, tdk)
struct thread *	tp;
thread_kstate *	tdk;
{
    void tstimeout();

    tstimeout(tp, tdk->tdk_timeout);
    tp->mx_sigvec = tdk->tdk_sigvec;
    tp->mx_svlen  = tdk->tdk_svlen;
    tp->mx_sig    = tdk->tdk_signal;
    tp->mx_local  = tdk->tdk_local;
    if (tp == curthread && tdk->tdk_signal)
    {
	thread_sig = tdk->tdk_signal;
	thread_sigpend = TDS_LSIG;
    }
}

/*
** savekstate - Save kernel state to thread descriptor.
*/
savekstate(tp, tdk)
struct thread *	tp;
thread_kstate *	tdk;
{
    long tgtimeout();

    tdk->tdk_timeout = tgtimeout(tp);
    tdk->tdk_sigvec = tp->mx_sigvec;
    tdk->tdk_svlen  = tp->mx_svlen;
    tdk->tdk_signal = tp->mx_lastsig;
    tdk->tdk_local  = tp->mx_local;
}

/*
** ThreadInit - Initialize routine for user threads.
**
** The code here is a bit tricky. Note that the thread descriptor
** pointer points to some data area on the stack of our parent, so
** we have to copy it to our own stack before going to sleep (and,
** moreover, this will fail horribly on a multiprocessor).
*/
void
threadinit_normal(tdi)
thread_idle *	tdi;
{
    register vir_bytes pc;		/* Why are these in registers? */
    register vir_bytes sp;

    pc = tdi->tdi_pc;
    sp = tdi->tdi_sp;
    /*
    ** The await here is just so that scheduling order is
    ** preserved. Our parent will immediately wake us up.
    */
    (void) await_reason((event) curthread, 0L, "threadinit");
    startuser((uint16) PROCSLOT(curthread->mx_process), pc, sp);
    /*
     * startuser is not required to return at all, but if it does
     * it means the thread should be terminated.
     */
    exitthread((long *) 0);
    /*NOTREACHED*/
}

/*
** threadinit_imm - start an immigrating thread.
**
** This is slightly more tricky.
** First, we load the kernel state, then we make a copy of the
** stack frame (ustate), and then we go to sleep. When we're woken
** we start the thread, which will immediately trap. Usertrap will
** then reload the registers and restart the thing.
*/
void
threadinit_imm(tdk)
thread_kstate *	tdk;
{
    thread_ustate *		tdu;
    thread_ustate *		intdu;
    int				tdusize;

    DPRINTF(0, ("threadinit_imm: thread_kstate = %x\n", tdk));
    intdu = (thread_ustate *) (tdk + 1);
    tdusize = USTATE_SIZE(intdu);
    if (tdu = (thread_ustate *) getblk((vir_bytes) tdusize))
	memmove((_VOIDSTAR) tdu, (_VOIDSTAR) intdu, (size_t) tdusize);
    loadkstate(curthread, tdk);
    (void) await_reason((event) curthread, 0L, "threadinit_imm");
    if (tdu)
    {
	curthread->mx_ustate = (vir_bytes) tdu;
	curthread->mx_flags |= TDS_START;
	thread_sigpend |= TDS_START;
    } else {
	/*
	** No memory was available.
	** Abort the thread.
	*/
	curthread->mx_flags |= TDS_HSIG;
	thread_sigpend |= TDS_HSIG;
    }
    startuser((uint16) PROCSLOT(curthread->mx_process),
					(vir_bytes) 0, (vir_bytes) 0);
    /*
     * startuser is not required to return at all, but if it does
     * it means the thread should be terminated.
     */
    exitthread((long *) 0);
    /*NOTREACHED*/
}

/*
** create_thread - helper routine to create a thread.
*/
create_thread(cp, tkd)
struct process *	cp;
thread_d *		tkd;
{
    register struct thread *tk;

    DPRINTF(0, ("create thread\n"));
    switch (tkd->td_extra) {
    case TDX_IDLE:
	/* Virgin thread. */
	if (tkd->td_len != sizeof (thread_d) + sizeof (thread_idle)) {
	    MON_EVENT("wrong length idle thread descriptor");
	    return STD_ARGBAD;
	}
	tk = NewThread(cp, threadinit_normal, (vir_bytes) (tkd + 1),
						(vir_bytes) KTHREAD_STKSIZ);
	break;
    case TDX_KSTATE | TDX_USTATE:
	/* Immigrating thread. */
	if (tkd->td_len < TDSIZE_MIN || tkd->td_len > TDSIZE_MAX) {
	    MON_EVENT("wrong length busy thread descriptor");
	    return STD_ARGBAD;
	}
	tk = NewThread(cp, threadinit_imm, (vir_bytes) (tkd + 1),
						(vir_bytes) KTHREAD_STKSIZ);
	break;
    default:
	MON_EVENT("wrong extra field in thread descriptor");
	return STD_ARGBAD;
    }
    if (tk == 0)
    {
	printf("NewThread: no room for thread\n");
	return STD_NOSPACE;
    }
    wakeup((event) tk);
    MON_EVENT("thread created");
    return STD_OK;
}

#define MIN_STACKSIZE	512   /* reasonable for all supported architectures */

/*
** newuthread - System call.
*/
int
newuthread(pc, sp, local)
vir_bytes	pc;
vir_bytes	sp;
long		local;
{
    thread_idle		tdi;
    struct thread *	tk;

    /* Before trying to create a new thread, make sure the supplied
     * stack is valid and big enough to let the new thread start at all.
     * If not, signal the calling thread.
     */
    if (umap(curthread, (vir_bytes) (sp - MIN_STACKSIZE),
	     (vir_bytes) MIN_STACKSIZE, 1) == 0)
    {
	progerror();
	return -1;
    }

    tdi.tdi_pc = pc;
    tdi.tdi_sp = sp;
    tk = NewThread(curthread->mx_process, threadinit_normal, (vir_bytes) &tdi,
						(vir_bytes) KTHREAD_STKSIZ);
    if (tk == 0)
    {
	MON_EVENT("sys_newthread failed");
	return -1;
    }
    tk->mx_local = local;
    wakeup((event) tk);
    MON_EVENT("sys_newthread");
    return 0;
}

/*
** getlocal - get glocal data pointer.
*/
long
getlocal()
{
    return curthread->mx_local;
}

int
sys_setlocal(ptr)
long **ptr;
{
    /* The process is telling us the address of its "_thread_local" pointer,
     * allowing us to keep it consistent during pre-emptive scheduling.
     *
     * Eventually this should be used during nonpre-emptive scheduling
     * as well, so that the user-level system call stubs don't have
     * to save and restore "_thread_local" anymore.
     *
     * Here we check that the pointer is valid using umap,
     * but we store the original value.  Just in case the process
     * changes its segments in a stupid way, we umap() the pointer
     * each time we actually use it to update "_thread_local".
     */
    phys_bytes kptr;

    if (ptr != 0) {
        kptr = umap(curthread, (vir_bytes)ptr, (vir_bytes)sizeof(long *), 1);
	if (kptr == 0)
	    return -1;
    } else {
	kptr = 0;
    }

    curthread->mx_process->pr_local_ptr = ptr;
#ifndef NO_FAST_UMAP
    /* also update mapped version */
    curthread->mx_process->pr_map_local_ptr = kptr;
#endif
    return 0;
}

/*
** Part 3: process creation/signalling routines.
*/

/*
** create_process - User interface routine to create a process.
*/
#ifdef __STDC__
errstat create_process(private *priv, process_d *cld, uint16 nbyte)
#else
errstat
create_process(priv, cld, nbyte)
private *	priv;
process_d *	cld;
uint16		nbyte;
#endif
{
    register struct process *	cl;
    register long		ret;
    register segment_d *	seg;
    thread_d *			tsk;
    register int		n;
    vir_bytes			totsize;

    DPRINTF(0, ("create_process\n"));
    /*
    ** Make the process.
    */
    if (strncmp(cld->pd_magic, ARCHITECTURE, ARCHSIZE) != 0)
    {
       MON_EVENT("wrong cpu type");
       return STD_ARGBAD;
    }
    if (pd_size(cld) != nbyte)
    {
	MON_EVENT("ill-formatted pd");
	return STD_ARGBAD;
    }
    if (cld->pd_nthread == 0 || cld->pd_nseg == 0)
    {
	MON_EVENT("nonsense pd");
	return STD_ARGBAD;
    }
    if ((cl = NewProcess()) == NILPROCESS) 
	return STD_NOSPACE;
    /*
    ** Invent a capability, and set owner.
    */
    uniqport(&cl->pr_random);
    ppro_setprv(cl, priv);
    cl->pr_owner = cld->pd_owner;
    cl->pr_signal = 0;
    /*
    ** Setup segments. First compute total
    ** size and call hintmap.
    */
    totsize = 0;
    for (seg = PD_SD(cld); seg < PD_SD(cld) + cld->pd_nseg; seg++)
    {
	seg->sd_type &= ~MAP_BINARY;
	if (seg->sd_type)
	    totsize += seg->sd_len;
    }
    hintmap((uint16) PROCSLOT(cl), totsize);

    /*
    ** Set up segments
    */
    ret = STD_OK;
    n = cld->pd_nseg;
    seg = PD_SD(cld);
    while (--n >= 0)
    {
	if (seg->sd_type && (ret = usr_mapseg(cl, seg)) != STD_OK) {
	    if (ret != STD_NOSPACE)
		ret = STD_SYSERR;
	    goto bad;
	}
	seg++;
    }
    assert(ret == STD_OK);

    /*
    ** Setup threads.
    */
    tsk = PD_TD(cld);
    for (n = 1; n <= (int) cld->pd_nthread; n++)
    {
	/*
	** Create the thread.
	*/
	if ((ret = create_thread(cl, tsk)) != STD_OK) {
	    if (cl->pr_nthread == 0)
		/* We got an error in creating the first thread, so we
		 * can still delete the process:
		 */
		goto bad;
	    else
		/* Too late to delete the process; have to signal it: */
	        break;
	}
	tsk = TD_NEXT(tsk);
    }
    /*
    ** The process is created. Register it in the ps directory
    ** and start it.
    */
    ppro_addobj(cl);
    if (ret != STD_OK)
    {
	/* There was an error in starting up one of the threads, so make the
	 * process exit as soon as possible (we can't call DelProcess once
	 * there are active threads in the process):
	 */
	MON_EVENT("process creation didn't complete");
	cl->pr_exitst = -1;
	cl->pr_flags |= CL_DYING;
	(void) memset((_VOIDSTAR) &cl->pr_owner, 0, sizeof (capability));
	/*
	** Kill all of its threads:
	*/
	assert (cl->pr_nthread != 0);
	sendall(cl, 0L, 0);
    }
    MON_EVENT("processes created");
    return ret;

bad:
    /*
    ** Something failed before any threads were activated.  We can safely
    ** delete the process since it never existed.
    */
    MON_EVENT("process not created");
    DelProcess(cl);
    return ret;
}

/*
** ppro_signal - (External) abort of a process.
*/
ppro_signal(priv, signo)
private *	priv;
signum		signo;
{
    void	stopprocess();

    struct process *cp;
    errstat		     err;

    if (priv == 0)
	cp = curthread->mx_process;
    else
	if ((err = ppro_getprv((rights_bits) PSR_KILL, priv, &cp)) != STD_OK)
	{
	    MON_EVENT("signalling invalid or non-existent process");
	    return err;
	}
    if (signo < 0) {
	if (cp->pr_flags & CL_DYING) {
	    MON_EVENT("signaling dying process");
	    return STD_NOTNOW;
	}
	cp->pr_flags |= CL_FASTKILL;
    } else if (cp->pr_flags & (CL_STOPPED|CL_DYING))
    {
	MON_EVENT("signalling stopped process");
	return STD_NOTNOW;
    }
    stopprocess(cp, signo);
    MON_EVENT("process signalled");
    return STD_OK;
}

/*
** ppro_setowner - (External) set owner of a process
*/
ppro_setowner(priv, newowner)
private *	priv;
capability *	newowner;
{
    struct process *cp;
    errstat		     err;

    if ((err = ppro_getprv((rights_bits) PSR_KILL, priv, &cp)) != STD_OK)
    {
	MON_EVENT("setowner for invalid or non-existent process");
	return err;
    }
    cp->pr_owner = *newowner;
    MON_EVENT("setowner of process");
    return STD_OK;
}

/*
** ppro_getowner - (External) set owner of a process
*/
ppro_getowner(priv, cur_owner)
private *	priv;
capability *	cur_owner;
{
    struct process *cp;
    errstat		     err;

    err = ppro_getprv((rights_bits) PSR_KILL, priv, &cp);
    switch (err)
    {
    case STD_OK:
	MON_EVENT("getowner of process");
	*cur_owner = cp->pr_owner;
	break;

    case STD_DENIED:
	/* Give them an owner cap with no rights */
	MON_EVENT("restricted getowner of process");
	err = std_restrict(&cp->pr_owner, (rights_bits) 0, cur_owner);
	break;

    default:
	MON_EVENT("getowner for invalid or non-existent process");
	break;
    }
    return err;
}

/*
** ppro_setcomment - Set the comment string in a process.
*/
ppro_setcomment(priv, str, len)
    private *priv;
    char *str;
    bufsize len;
{
    struct process *cp;
    errstat		     err;

    if ((err = ppro_getprv((rights_bits) PSR_KILL, priv, &cp)) != STD_OK)
    {
	MON_EVENT("setcomment for invalid or non-existent process");
	return err;
    }
    if (len >= PSVR_COMMENTSIZ) len = PSVR_COMMENTSIZ-1;
    str[len] = 0;
    (void) strcpy(cp->pr_comment, str);
    return STD_OK;
}

/*
** ppro_getcomment - Get the comment string in a process.
*/
ppro_getcomment(priv, strp, lenp)
    private *priv;
    char **strp;
    bufsize *lenp;
{
    struct process *cp;
    errstat err;

    if ((err = ppro_getprv((rights_bits) 0, priv, &cp)) != STD_OK)
    {
	MON_EVENT("getcomment for invalid or non-existent process");
	return err;
    }
    *strp = cp->pr_comment;
    *lenp = strlen(cp->pr_comment)+1;
    return STD_OK;
}

	

/*-----------------------------------------------------------------
**
** Part 4: miscellaneous.
**
**-----------------------------------------------------------------
*/

/*
** GetThreadMap - get a pointer to the mapping descriptor.
*/
struct mapping *
GetThreadMap(t)
register struct thread *t;
{
    assert(t);
    assert(t->mx_process);
    return t->mx_process->pr_map;
}

/*
** getinfo - system call.
** Get process capability and process descriptor.
**
** This is a crock. Sape will have to come up with something
** more reasonable soon. BUG
*/
long
getinfo(ccp, cdp, cdplen)
capability *	ccp;
process_d *	cdp;
int		cdplen;
{
    long	buildpd();

    register struct process *cl = curthread->mx_process;

    if (ccp != 0)
    {
	ppro_setprv(cl, &ccp->cap_priv);
	ccp->cap_port = segsvr_put_port;
    }
    return cdp != 0 ? buildpd(cl, cdp, cdplen, 0, 0) : 0L;
}
