/*	@(#)trap.c	1.12	96/02/27 13:45:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * trap.c
 *
 * Handle i80386 CPU traps and some other small stuff (like setting device
 * vectors, generic kernel profiling, and Amoeba's signal handling). Since
 * Amoeba doesn't do demand paging its trap handling strategy is very simple:
 * whenever a trap occurs when a user thread is running, send a signal to
 * the associated process, and when the trap occurs when in kernel mode, just
 * panic. This strategy is somewhat blurred by the existence of co-processor
 * and kernel debugger trap handling.
 *
 * The floating point mechanism for 387/487 co-processor's on Amoeba works
 * as follows: Initially all co-processor instructions are trapped (the
 * task switch bit is set). This trap ends up in the routine below which
 * assigns the co-processor to the faulting thread and turns off the
 * emulate bit for that thread only (this is done by common_interrupt).
 * When another thread uses a co-processor instruction that too ends up
 * in the trap routine below and will cause it to save the co-processor
 * state and restore/initialize the state for the new thread. Finally it
 * assigns the co-processor to that thread.
 *
 * The advantage of this scheme is that the number of ndp state saves and
 * stores are reduced to their essential minimum. The worst case is still
 * one state save/restore per thread switch), but the average case is much
 * better.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <bool.h>
#include <assert.h>
INIT_ASSERT
#include <stddef.h>
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <exception.h>
#include <process/proc.h>
#include <syscall.h>
#include <module/buffers.h>
#include <arch_proto.h>
#include <ndp.h>
#include <fault.h>
#include <global.h>
#include <trap.h>
#include <i80386.h>
#include <machdep.h>
#include <map.h>
#include <type.h>
#include <kthread.h>
#include <sys/proto.h>
#include <debug.h>

/*
 * The following two type variables (cpu and ndp type) should
 * be initialized so that clearbss will not overwrite them.
 */
int cpu_type = CPU_NONE;
int ndp_type = NDP_NONE;

struct thread *ndpthread;	/* thread that "owns" ndp */

#ifdef STATISTICS
static int ndp_swtchcount;	/* ndp switch count */
static int ndp_ovrcount;	/* ndp overrun count */
static int ndp_errcount;	/* ndp error count */
#endif

void ndp_save(), ndp_restore(), ndp_reinit(), ndp_clearerr();

#ifdef KGDB
extern int kgdb_enabled;
#endif

/*
 * Processor traps
 */
static struct {
    signum e_sig;
    char *e_name;
} error[] = {
    EXC_DIV,	"division by zero",		/* 00 */
    EXC_BPT,	"debug exceptions",		/* 01 */
    EXC_NONE,	"NMI",				/* 02 */
    EXC_BPT,	"breakpoint",			/* 03 */
    EXC_DIV,	"overflow",			/* 04 */
    EXC_DIV,	"bounds check",			/* 05 */
    EXC_INS,	"invalid opcode",		/* 06 */
    EXC_INS,	"device not available",		/* 07 */
    EXC_NONE,	"double fault",			/* 08 */
    EXC_ACC,	"coprocessor segment overrun",	/* 09 */
    EXC_ACC,	"invalid TSS",			/* 10 */
    EXC_ACC,	"segment not present",		/* 11 */
    EXC_ACC,	"stack fault",			/* 12 */
    EXC_ACC,	"general protection",		/* 13 */
    EXC_ACC,	"page fault",			/* 14 */
    EXC_NONE,	"trap 15",			/* 15 */
    EXC_DIV,	"floating-point error",		/* 16 */
    EXC_INS,	"alignment check",		/* 17 (486 only) */
};

#if defined(ISA) || defined(MCA)
void nmitrap();
void kbd_reboot();
#endif

extern void setCR0();
void framedump();

/*
 * Trap uninitialized interrupts
 */
/*ARGSUSED*/
void
nullvect(irq, frame)
    int irq;
    struct fault *frame;
{
    assert(irq >= 0 && irq < NIRQ);
    printf("Null IRQ vector %d\n", irq);
}

extern void (*ivect[])();

/*
 * Set the IRQ vector
 */
void
setirq(irq, handler)
    int irq;
    void (*handler)();
{
    if (ivect[irq] != nullvect)
	panic("setirq: IRQ vector %d multiply used\n", irq);
    ivect[irq] = handler;
}

/*
 * Reset an IRQ vector to the default nullvect.
 */
void
unsetirq(irq)
    int irq;
{
    if (ivect[irq] == nullvect)
	printf("unsetirq: IRQ vector %d not set\n", irq);
    ivect[irq] = nullvect;
}

#ifdef PROFILE
/*
 * i80386 dependent code for the kernel profile server and user profiling
 */
static char *profile_head, *profile_tail, *profile_ptr;
static void (*profile_done) _ARGS((int res));
static int prof_users; /* number of processes currently profiling */

static void
profile_do_start(milli)
long milli;
{
#if defined(ISA) || defined(MCA)
    cmos_prof_start(milli);
#endif
    prof_users++;
}

static void
profile_do_stop()
{
    if (--prof_users == 0) {
#if defined(ISA) || defined(MCA)
	DPRINTF(1, ("cmos_prof_stop\n"));
	cmos_prof_stop();
#endif
    }
}

errstat
profile_start(buffer, size, milli, done)
    char *buffer;
    bufsize size;
    long milli;
    void (*done)();
{
    profile_head = profile_ptr = buffer;
    profile_tail = buffer + size;
    profile_done = done;
    profile_do_start(milli);
    return STD_OK;
}

void
profile_stop()
{
    profile_do_stop();
}

errstat
user_prof_start(milli)
long milli;
{
    profile_do_start(milli);
    return STD_OK;
}

void
user_prof_stop()
{
    profile_do_stop();
}

void
profile_timer(frame)
    register struct fault *frame;
{
    register long *ebp, *oebp;
    char *prof_ptr, *prof_end;

    if (frame->if_cs == K_CS_SELECTOR && frame->if_ds == K_DS_SELECTOR) {
	if (profile_ptr == NULL) {
	    /* This can happen during user profiling */
	    return;
	}

	prof_ptr = profile_ptr;
	prof_end = profile_tail;

	/* initial program counter */
	prof_ptr = buf_put_uint32(prof_ptr, prof_end, frame->if_eip);

	/* walk through stack frame, extracting all program counters */
	ebp = (long *)frame->if_ebp;
	if (ebp != (long *)0) {
	    do {
		if (!probe((vir_bytes)ebp[1], 4))
		    break;
		prof_ptr = buf_put_uint32(prof_ptr, prof_end, ebp[1]);
		oebp = ebp;
		ebp = (long *)ebp[0];
	    } while (ebp > oebp);
	}
        prof_ptr = buf_put_uint32(prof_ptr, prof_end, 0L);

	if (!prof_ptr) { /* ignore the last trace */
	    (*profile_done)(profile_ptr - profile_head);
	    return;
	}
	profile_ptr = prof_ptr;
    } else {
	if ((frame->if_cs & 0xFFFF) == U_CS_SELECTOR &&
	    (frame->if_ds & 0xFFFF) == U_DS_SELECTOR)
	{
	    user_prof_tick((vir_bytes) frame->if_eip);
	}
	else
	{
	    DPRINTF(1, ("profile_timer: %d: cs = 0x%lx; ds = 0x%lx\n",
		        THREADSLOT(curthread), frame->if_cs, frame->if_ds));
        }
    }
}
#endif /* PROFILE */

#if !defined(NDEBUG)
int
interrupts_enabled()
{
    int f;

    return (get_flags() & EFL_IF);
}
#endif /* NDEBUG */

/*
 * Reboot machine. On an ISA/MCA architecture the planar chip is used
 * to reset the machine. For other bus architecture we just hang. For
 * coldstart kernels we just hang to give the user a chance to look at
 * the panic messages.
 */
void
reboot()
{
    disable();
#if !defined(NOREBOOT) && !defined(COLDSTART)
#if defined(ISA) || defined(MCA)
    if (kernel_option("noreboot") == 0)
	kbd_reboot();
#endif /* defined(ISA) || defined(MCA) */
#endif /* !defined(NOREBOOT) && !defined(COLDSTART) */
    printf("Press the reset button to reboot.\n");
    for (;;) /* hang */;
}

/*
 * Kernel abort just reboots
 */
void
abort()
{
    printf("Kernel abort called.\n");
    stop_kernel();
    reboot();
}

#ifndef NOPROC
/*
 * Return from a signal handler. Saved the fault frame to return to
 * in callcatcher() when were jumping to the handler. In as.S left a
 * pointer to the frame in if_edx of the present fault frame. Copy
 * the saved frame into the present frame.
 */
int
sigreturn(frame)
    struct fault *frame;
{
    struct fault *nframe;

    /* was saved here after call to newsys() */
    nframe = (struct fault *) frame->if_edx;
    if (nframe == (struct fault *)0)
	return 0;
    *frame = *nframe; /* frame to return to */
    return 1;
}

/*
 * Call a signal handler. Need to save the present fault frame so will
 * know what to return to when the handler has finished. Also set up
 * stack for handler by pushing a catchframe. The routine _sys_catcher
 * will call the handler and then call sys_sigret() with a pointer to
 * the fault frame as an argument.
 */
int
callcatcher(trapno, sv, frame)
    long trapno;
    sig_vector *sv;
    struct fault *frame;
{
    struct catchframe {
	long c_dummyretaddr;	/* _sys_catcher must never return */
	long c_trapno;		/* arg1: trap number */
	long c_faddr;		/* arg2: fault address */
	long c_arg3;		/* arg3: user specified (address of handler) */
	long c_arg4;		/* arg4: user specified (argument to handler) */
	struct fault c_fault;	/* a real fault struct */
    } *cf;

    /*
     * Increase user stack to cope with catchframe
     */
    cf = (struct catchframe *) umap(curthread,
	(vir_bytes) (frame->if_esp - sizeof(*cf)), (vir_bytes) sizeof(*cf), 1);
    if (cf == (struct catchframe *)0)
	return 0;

    cf->c_fault = *frame;
    cf->c_arg4 = sv->sv_arg4;
    cf->c_arg3 = sv->sv_arg3;
    cf->c_faddr = (long)frame->if_esp - sizeof(struct fault);
    cf->c_trapno = trapno;
    /* _sys_catcher should not return but should call sys_sigret */
    cf->c_dummyretaddr = -1;
    frame->if_esp = frame->if_esp - sizeof(*cf);
    frame->if_ebp = frame->if_esp;
    frame->if_eip = sv->sv_pc;	/* _sys_catcher */
    return 1;
}

/*
 * Handle software trap. Hardware traps also end up here, indirectly:
 * trap sends a signal to the thread taking the exception and then
 * returns, causing swtrap to be called.
 */
#define	UFLAGS (EFL_CF|EFL_PF|EFL_AF|EFL_ZF|EFL_NF|EFL_DF|EFL_OF|EFL_AC|EFL_TF)

/*ARGSUSED*/
int
swtrap(signo, reason, frame)
    signum signo;
    int reason;
    struct fault *frame;
{
    extern long thread_sigpend;
    uint32 eflag, memmap;

    assert(frame->if_cs == U_CS_SELECTOR);

    /*
     * Save the floating point registers when handling
     * a trap. Note that we never get here on a floating
     * point exception.
     */
    if (ndpthread == curthread) {
	assert(reason != TRAP_NDPNA);
	assert(reason != TRAP_OVERRUN);
	assert(reason != TRAP_NDPERROR);
	ndp_save(ndpthread->md_ndpstate);
	ndpthread->md_ndpvalid = TRUE;
	ndpthread = (struct thread *) 0;
	setCR0(getCR0() | CR0_TS);
    }

    /*
     * Deal with the ordinary trampoline code.
     * Make sure the user didn't change any of
     * the harmful fault frame registers.
     */
    eflag = frame->if_eflag;
    memmap = frame->if_memmap;	/* save CR3 */
    if (thread_sigpend & TDS_SRET) {
	/* returning from signal handler */
	thread_sigpend &= ~TDS_SRET;
	if (!sigreturn(frame))
	    return 0;
    } else  {
	if (!usertrap(signo, frame))
	    return 0;
    }

    /* make sure user didn't change them */
    frame->if_cs = U_CS_SELECTOR;
    frame->if_ds = U_DS_SELECTOR;
    frame->if_es = U_DS_SELECTOR;
    frame->if_gs = U_DS_SELECTOR;
    frame->if_fs = U_DS_SELECTOR;
    frame->if_ss = U_DS_SELECTOR;
    frame->if_eflag = (frame->if_eflag & UFLAGS) | (eflag & ~UFLAGS);
    frame->if_memmap = memmap;
    return 1;
}
#endif /* NOPROC */

/*
 * Handle i80386 CPU traps. For some cases we try to do something
 * sensible with the trap (e.g. kernel debugger or co-processor
 * handling) in all other cases we just send a signal to the faulting
 * user process or panic when it is caused by the kernel itself.
 */
int
trap(reason, frame)
    int reason;
    struct fault *frame;
{
    static long trapcount = 0;
    unsigned long ss, esp;
    signum usig;

    /*
     * A trap during a trap (while in kernel space) is considered fatal.
     * The second if is used to catch traps caused by calling routines
     * from the trap handler (e.g. by printf).
     */
    if (trapcount == 1) {
	trapcount++;
	printf("FATAL\n");
	exitkernel();
    } else if (trapcount > 1)
	exitkernel();

    /*
     * Complete fault frame; some registers need extra masking and
     * estimated guesses can be made when the frame is incomplete.
     */
    frame->if_cs &= 0xFFFF;
    frame->if_ds &= 0xFFFF;
    frame->if_es &= 0xFFFF;
    frame->if_fs &= 0xFFFF;
    frame->if_gs &= 0xFFFF;

    /*
     * No privilege transition, make up our own ss/esp.
     * WE CANNOT USE if_esp AND if_ss IN THAT CASE !
     */
    if (frame->if_cs == K_CS_SELECTOR) {
	ss = frame->if_ds;
	esp = frame->if_faddr;
    } else {
	frame->if_ss &= 0xFFFF;
	ss = frame->if_ss;
	esp = frame->if_esp;
    }

    /*
     * Deal with the trap ...
     */
    switch (reason) {

#if defined(ISA) || defined(MCA)
    /*
     * Non maskable interrupt
     */
    case TRAP_NMI:
	nmitrap(reason, frame);
	return 0;
#endif

    /*
     * Co-processor related traps
     */
    case TRAP_NDPNA:
	return ndp_notavail(reason, frame);
    case TRAP_OVERRUN:
	return ndp_overrun(reason, frame);
    case TRAP_NDPERROR:
	return ndp_error(reason, frame);

    /*
     * Any other trap. When a user process caused it we send an
     * appropriate signal to it. Otherwise, i.e. the kernel caused
     * it, we panic and dump the contents of the current registers.
     */
    default:
	if (reason >= 0 && reason <= (sizeof(error)/sizeof(error[0])))
	    usig = error[reason].e_sig;
	else
	    usig = EXC_NONE;
	if (frame->if_cs == U_CS_SELECTOR && usig != EXC_NONE) {
	    /* trap occurred while in user mode */
	    putsig(curthread, usig);
	    return 1; /* as.S will send us to swtrap */
	} else {
	    /* trap occurred while in kernel mode */
#ifdef KGDB
	    /* call kernel debugger when it is enabled */
	    if (kgdb_enabled) return kgdb_trap(reason, frame);
#endif
	    trapcount++;
	    printf("\nTRAP %d: %s\n", reason,
		usig != EXC_NONE ? error[reason].e_name : "UNKNOWN");
	    framedump(frame, ss, esp);
	    if (frame->if_ds == K_DS_SELECTOR && ss == K_DS_SELECTOR)
		stackdump((char *) frame->if_eip, (long *) frame->if_ebp);
	    else
		printf("Sorry, no stackdump. cs=%x, ds=%x, ss=%x\n",
		    frame->if_cs, frame->if_ds, ss);
	    exitkernel();
	}
    }
    /*NOTREACHED*/
}

/*
 * Handle numeric co-processor not available traps
 */
/*ARGSUSED*/
int
ndp_notavail(reason, frame)
    int reason;
    struct fault *frame;
{
    /*
     * Any co-processor instruction is an illegal opcode
     * when the device is not installed.
     */
    if (ndp_type == NDP_NONE) {
#ifdef EMULATOR
	if ((getCR0() & CR0_EM) == CR0_EM)
	    emulator(frame);
	else
#endif /* EMULATOR */
	    putsig(curthread, (signum) EXC_INS);
	return 1;
    }

    /* a number of sanity checks */
    assert(ndpthread != curthread);
    assert((getCR0() & CR0_EM) == 0);
    assert((getCR0() & CR0_TS) == CR0_TS);

    /* stop trapping co-processor instructions */
    setCR0(getCR0() & ~CR0_TS);

    /*
     * The current thread does not "own" the co-processor,
     * save its state on a per thread basis and restore the
     * state of the previous thread (if any).
     */
    if (ndpthread) {
	ndp_save(ndpthread->md_ndpstate);
	ndpthread->md_ndpvalid = TRUE;
    }

    /*
     * Assign a new thread to the co-processor,
     * possibly restoring a previous saved state.
     */
    ndpthread = curthread;
    if (ndpthread->md_ndpvalid) {
	ndp_restore(ndpthread->md_ndpstate);
	ndpthread->md_ndpvalid = FALSE;
    } else
	ndp_reinit();

#ifdef STATISTICS
    ndp_swtchcount++;
#endif

    return 1;
}

/*
 * Handle page faults that are caused by co-processor
 * instructions. Re-initialize the ndp and send an
 * exception signal to the faulting thread.
 */
/*ARGSUSED*/
int
ndp_overrun(reason, frame)
    int reason;
    struct fault *frame;
{
    assert((getCR0() & CR0_EM) == 0);

    /* stop trapping co-processor instructions */
    setCR0(getCR0() & ~CR0_TS);

    if (ndpthread) {
	/* save state for debugger */
	ndp_save(ndpthread->md_ndpstate);
	ndpthread->md_ndpvalid = FALSE;
	ndp_reinit();
	putsig(ndpthread, (signum) EXC_ACC);
	ndpthread = (struct thread *) 0;
    } else
	putsig(curthread, (signum) EXC_DIV);

#ifdef STATISTICS
    ndp_ovrcount++;
#endif

    /* trap co-processor instructions */
    setCR0(getCR0() | CR0_TS);
    return 1; /* as.S will send us to swtrap */
}

/*
 * Handle co-processor error by clearing the error
 * status and sending an exception signal to the
 * faulting thread.
 */
/*ARGSUSED*/
int
ndp_error(reason, frame)
    int reason;
    struct fault *frame;
{
    assert((getCR0() & CR0_EM) == 0);

    /* stop trapping co-processor instructions */
    setCR0(getCR0() & ~CR0_TS);

    if (ndpthread) {
	/* save state for debugger */
	ndp_save(ndpthread->md_ndpstate);
	ndpthread->md_ndpvalid = FALSE;
	ndp_clearerr();
	putsig(ndpthread, (signum) EXC_DIV);
	ndpthread = (struct thread *) 0;
    } else
	putsig(curthread, (signum) EXC_DIV);

#ifdef STATISTICS
    ndp_errcount++;
#endif

    /* trap co-processor instructions */
    setCR0(getCR0() | CR0_TS);
    return 1; /* as.S will send us to swtrap */
}

/*
 * Clean up ndp driver state information
 */
void
ndp_cleanup(tp)
    struct thread *tp;
{
    tp->md_ndpvalid = FALSE;
    if (ndpthread == tp) {
	ndpthread = (struct thread *) 0;
	setCR0(getCR0() | CR0_TS);
    }
}

#ifdef STATISTICS
/*
 * Print ndp switching statistics
 */
int
ndp_dump(begin, end)
    char *begin, *end;
{
    register char *p;

    p = bprintf(begin, end, "======== NDP statistics =======\n");
    p = bprintf(p, end, "%d context switches\n", ndp_swtchcount);
    p = bprintf(p, end, "%d ndp overruns\n", ndp_ovrcount);
    p = bprintf(p, end, "%d ndp errors\n", ndp_errcount);
    return p - begin;
}
#endif /* STATISTICS */

/*
 * Dump stack back trace. Often called after a panic.
 * "foo" is used to retrieve the stack and base pointer.
 */
static void
_stacktrace(arg)
    long arg;
{
    long *foo = &arg;
    stackdump((char *) foo[-1], (long *) foo[-2]);
}

void
stacktrace()
{
    long dummy = 0;
    _stacktrace(dummy);
}

/*
 * Do a stack back trace
 */
stackdump(eip, ebp)
    char *eip;
    long *ebp;
{
    register int i, narg;
    int guessing;
    long *oebp;
    unsigned char *neip;

    if (ebp != (long *)0) do {
	narg = 0;
	guessing = 0;
	if ((neip = (unsigned char *)ebp[1]) != (unsigned char *)0) {
	    if ((neip[0] & 0xF8) == 0x58){
		if ((neip[1] & 0xF8) == 0x58)
		    narg = 2; /* pop ecx; pop ecx */
		else
		    narg = 1; /* pop ecx */
	    } else if (neip[0] == 0x83 && neip[1] == 0xC4) /* add sp,XX */
		narg = neip[2] / 4;
	    if (narg > 7) {
		narg = 7;
		guessing++;
	    }
	}
	printf("%x: %x(", ebp, eip);
	for(i = 0; i < narg; i++)
	    printf("%s%x", i == 0 ? "": ", ", ebp[i+2]);
	if (guessing) printf(", ...");
	printf(")\n");
	eip = (char *) neip;
	oebp = ebp;
	ebp = (long *)ebp[0];
    } while (ebp > oebp);
    printf("%x: called from %x\n", ebp, eip);
}

/*
 * Register frame dump
 */
void
framedump(frame, ss, esp) 
    struct fault *frame;
    uint32 ss, esp;
{
    printf("cs:eip           %8x:%8x\n",
	(frame->if_cs & 0xFFFF), frame->if_eip); 
    if (ss == -1) {
	/* coming from keyboard.c */
	printf("ss:esp           %8x:%8x\n",
	    (frame->if_ds & 0xFFFF), frame->if_faddr);
    } else {
	/* coming from trap.c */
	printf("ss:esp           %8x:%8x\n", ss, esp);
    }
    printf("eflag            %8x\n", frame->if_eflag);
    printf("ebp              %8x\n", frame->if_ebp);
    printf("ds/es/fs/gs      %8x %8x %8x %8x\n",
	(frame->if_ds & 0xFFFF), (frame->if_es & 0xFFFF),
	(frame->if_fs & 0xFFFF), (frame->if_gs & 0xFFFF));
    printf("eax/ebx/ecx/edx  %8x %8x %8x %8x\n",
	frame->if_eax, frame->if_ebx, frame->if_ecx, frame->if_edx);
    printf("edi/esi          %8x %8x\n", frame->if_edi, frame->if_esi);
    printf("cr3              %8x\n", frame->if_memmap);
    printf("faultaddr        %8x\n", getCR2());
    printf("code             %8x\n", 
	frame->if_trap == 13 ? frame->if_errcode & 0xFFFF : frame->if_errcode);
}
