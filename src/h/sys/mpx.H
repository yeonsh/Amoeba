/*	@(#)mpx.H	1.8	96/02/27 10:39:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  WARNING:
**	This file is included by kthread.h directly into a struct definition
**	As a consequence you cannot define anything except macros and
**	additions inside the mpx struct.  This makes the struct definition
**	a little messy, but such is life.
*/

#define PSVR_COMMENTSIZ 32
#define	NUM_PRIORITIES	8

/* pr_flags: */
#define CL_STOPPED	0x0001		/* Stopped, because of exception */
#define CL_DYING	0x0002		/* Exitprocess processing */
#define CL_WARNED	0x0004		/* Pd sent to owner */
#define CL_FASTKILL	0x0008		/* Kill process immediately */
#define	CL_PREEMPTIVE	0x0010		/* Schedule preemptively */

#ifndef NO_FAST_UMAP
#define NUM_UMAP_HASH_BITS	5
#define NUM_UMAP_HASH		(1 << NUM_UMAP_HASH_BITS)
#endif

struct mpx
{
    vir_bytes		MX_sp;		/* sp of this thread MUST BE FIRST*/
    vir_bytes		MX_stbot;	/* base of the stack */
#ifndef NDEBUG
    vir_bytes		MX_sttop;	/* top of stack */
#endif /* NDEBUG */
    unsigned long	MX_flags;	/* Flags - see below */
    long		MX_sig;		/* Signal received */
    long		MX_lastsig;	/* Ditto */
    long		MX_sigvec;	/* Address of signal vector */
    long		MX_svlen;	/* Sigvec length */
    event   		MX_event;	/* event pointer */
    long		MX_ustate;	/* user state pointer */
    interval		MX_timeout;	/* Timeout for await() */
#ifdef AWAIT_DEBUG
    char               *MX_awaitreason;	/* Reason for calling await */
    char               *MX_prevreason;	/* Previous reason */
#endif
    /* New mutex implementation uses one extra level of indirection:
     * the "mutex reference".  In the new implementation, a mutex no longer
     * contains the thread number of the first thread waiting on it.
     * Its only predefined values are zero (meaning that it is free) or
     * non-zero (meaning that some thread has locked it).
     */
    struct mu_ref {
	struct mu_ref   *mr_next;	/* Links mu_refs within a process */
	mutex		*mr_mutex;	/* Address of the mutex */
	struct thread   *mr_first;	/* First thread blocked on mutex */
	struct thread   *mr_last;	/* Last thread block on it */
    }			*MX_mu_ref;
    struct thread	*MX_mu_next;	/* Next thread with same mu_ref */
    long		MX_priority;	/* Thread's priority */
    struct thread	*MX_nextthread;	/* next thread for this process */
    struct process
    {
	vir_bytes		pr_cap;		/* Capability pointer */
	struct mapping
	{
	    struct mapping  *mp_next;	/* Next mapping */
	    struct segtab   *mp_seg;	/* Segment being mapped */
	    struct process  *mp_clus;	/* The process it's mapped in */
	    vir_clicks	    mp_addr;	/* Address */
	    vir_clicks	    mp_len;	/* and length */
	    long	    mp_type;	/* and type */
	}
				*pr_map;	/* Mapping descriptor */
#ifndef NO_FAST_UMAP
	struct mapping		*pr_map_index[NUM_UMAP_HASH];
#endif
	struct process		*pr_nextcl;	/* Next process */
	struct thread		*pr_prioqs[NUM_PRIORITIES]; /* queue per prio */
	long		       **pr_local_ptr;	/* address of _thread_local */
#ifndef NO_FAST_UMAP
	phys_bytes		pr_map_local_ptr; /* umap(pr_local_ptr) */
#endif
	uint16			pr_slotno;	/* Slot in processes array */
	uint16			pr_nthread;	/* Number of threads */
	uint16			pr_ndying;	/* Number of dying threads */
	uint16			pr_nrun;	/* Number of runnable threads */
	uint16			pr_nstop;	/* Number of stopped threads */
	uint16			pr_flags;	/* Some random bits. */
	signum			pr_signal;	/* Signal received */
	long			pr_exitst;	/* exit status */
	capability		pr_owner;	/* The process owner */
	capability		pr_symtab;	/* Symbol table */
	port			pr_random;	/* Our random part */
	char			pr_comment[PSVR_COMMENTSIZ];
					/* Comment string */
#if KERNEL_GLOCALS
	int			pr_glocals;	/* Number of glocal types */
#endif
	struct mu_ref	       *pr_muref;	/* first mutex reference */
#ifdef PROFILE
	vir_bytes		pr_prof_buf;
	vir_bytes		pr_prof_size;
	vir_bytes	        pr_prof_pclow;
	int			pr_prof_scale;
#endif
    }
			*MX_process;	/* our process */
    long		MX_local;	/* glocal data pointer */
#if KERNEL_GLOCALS
    struct glocal
    {
	char **gtable;
	int gtable_size;
    } 			MX_glocal;	/* glocal data for kernel threads */
#endif
} tk_mpx;

#ifndef AWAIT_DEBUG
#define await_reason(ev, time, reason)	await(ev, time)
#endif

#ifdef MPX

#define mx_sp		tk_mpx.MX_sp
#define mx_cap		tk_mpx.MX_cap
#define mx_stbot	tk_mpx.MX_stbot
#ifndef NDEBUG
#define mx_sttop	tk_mpx.MX_sttop
#endif /* NDEBUG */
#define mx_flags	tk_mpx.MX_flags
#define mx_sigvec	tk_mpx.MX_sigvec
#define mx_sig		tk_mpx.MX_sig
#define mx_lastsig	tk_mpx.MX_lastsig
#define mx_svlen	tk_mpx.MX_svlen
#define mx_event	tk_mpx.MX_event
#define mx_timeout	tk_mpx.MX_timeout
#define mx_ustate	tk_mpx.MX_ustate
#define mx_muref	tk_mpx.MX_mu_ref
#define mx_munext	tk_mpx.MX_mu_next
#define	mx_priority	tk_mpx.MX_priority
#define mx_nextthread	tk_mpx.MX_nextthread
#define mx_process	tk_mpx.MX_process
#define mx_local	tk_mpx.MX_local
#if KERNEL_GLOCALS
#define mx_glocal	tk_mpx.MX_glocal
#endif /* KERNEL_GLOCALS */

#else /* MPX */

#define tk_mpx		tk_dummy	/* other modules must not touch it */

#endif /* MPX */

/* Bits for mx_status are in proc.h */

#define NILPROCESS (struct process *)0

#define PROCSLOT(cl)	((cl)->pr_slotno)
