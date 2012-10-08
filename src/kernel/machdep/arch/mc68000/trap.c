/*	@(#)trap.c	1.9	96/02/27 13:46:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>

#include <amoeba.h>
#include <stderr.h>
#include <exception.h>
#include <sys/proto.h>
#include <module/buffers.h>

#include <machdep.h>
#include <arch_proto.h>

#include "global.h"
#include "kthread.h"
#include "fault.h"
#include "map.h"
#include "process/proc.h"
#include "as.h"
#include "trap.h"
#include "debug.h"

extern long thread_sigpend;

#define NVEC	256		/* 256 interrupt vectors */

#ifdef STACKFRAME_68000
short is68000;			/* set when this is an 68000 */

struct formatF errorframe;	/* 68000 bus and address error frame */
#endif /* STACKFRAME_68000 */

int is_mc68030;			/* Dynamically determined */
int has_fp;			/* .. */

unsigned mc68000_fsize[] = {		/* format sizes */
	8, 8, 12, 8, 8, 8, 8, 8, 58, 20, 32, 92, 8, 8, 8, 8
};

static struct intname {
    signum e_sig;
    char *e_name;
} error[] = {
	EXC_NONE,	"0",
	EXC_NONE,	"0",
	EXC_MEM,	"bus err",
	EXC_ODD,	"addr err",
	EXC_ILL,	"ill instr",
	EXC_DIV,	"zero div",
	EXC_INS,	"chk",
	EXC_FPE,	"trapv",
	EXC_ACC,	"priv viol",
	EXC_BPT,	"trace trap",
	EXC_INS,	"line 1010",
	EXC_INS,	"line 1111",
	EXC_NONE,	"12",
	EXC_ARG,	"coproc prot viol",
	EXC_ARG,	"fmt err",
	EXC_NONE,	"uninit vector",
	0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0,		/* 16-23 */
	0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0,		/* 24-31 */
	EXC_EMU,	"trap 0",
	EXC_BPT,	"trap 1",	/* Trap #1 is breakpoint */
	EXC_EMU,	"trap 2",
	EXC_EMU,	"trap 3",
	EXC_EMU,	"trap 4",
	EXC_EMU,	"trap 5",
	EXC_EMU,	"trap 6",
	EXC_EMU,	"trap 7",
	EXC_EMU,	"trap 8",
	EXC_EMU,	"trap 9",
	EXC_EMU,	"trap 10",
	EXC_EMU,	"trap 11",
	EXC_EMU,	"trap 12",
	EXC_EMU,	"trap 13",
	EXC_EMU,	"trap 14",
	EXC_EMU,	"trap 15",
};

char *space[] = {
	"0",
	"user data",
	"user program",
	"control space",
	"4",
	"kernel data",
	"kernel program",
	"cpu space"
};

#ifndef NDEBUG
long intcount[NVEC];		/* Array where interrupts are counted */
#endif				/* should have a way to read it */

void (*intswitch[NVEC]) _ARGS(( int ));

#ifdef XXPROFILE
extern void profint();
#endif

struct vector {
	short v_vector;
	void (*v_catch) _ARGS(( void ));
} vectors[] = {
#ifdef XXPROFILE
  TRAPV_AUTOVEC(7),	profint,	/* nmi: profiling		*/
#endif
#ifndef NOPROC
  TRAPV_TRAP( 0),       urpc_getreq,    /* trap 0                       */
  TRAPV_TRAP( 2),       urpc_trans,     /* trap 2                       */
  TRAPV_TRAP( 3),       urpc_putrep,    /* trap 3                       */
  TRAPV_TRAP( 1),	0,		/* trap 1	(breakpoint)	*/
  TRAPV_TRAP( 4),	ugetreq,	/* trap 4	(getreq)	*/
  TRAPV_TRAP( 5),	uputrep,	/* trap 5	(putrep)	*/
  TRAPV_TRAP( 6),	utrans,		/* trap 6	(trans)		*/
  TRAPV_TRAP( 7),	usuicide,	/* trap 7	(exitthread)	*/
  TRAPV_TRAP(10),	utimeout,	/* trap 10	(timeout)	*/
  TRAPV_TRAP(11),	ucleanup,	/* trap 11	(cleanup)	*/
  TRAPV_TRAP(12),	uawait,		/* trap 12	(await)		*/
  TRAPV_TRAP(13),	unewsys,	/* trap 13	(new syscalls)	*/
  TRAPV_TRAP(14),	umutrylock,	/* trap 14			*/
  TRAPV_TRAP(15),	umuunlock,	/* trap 15			*/
#endif
  0,			0
};

void inittrap _ARGS(( void ));

static void printfault _ARGS(( int /* uint16 */ reason, struct fault *sp ));
static void spurious _ARGS(( struct stframe stframe ));
static int try_and_catch _ARGS(( long (*rout)(vir_bytes addr), long arg, 
						long *result, int vector ));
static long probestub _ARGS(( vir_bytes addr ));
static long wprobestub _ARGS(( vir_bytes addr ));

#ifdef PROFILE
static char *profile_head, *profile_tail, *profile_ptr;
static void (*profile_done) _ARGS(( int /* bufsize */ res ));
static int prof_users; /* number of processes currently profiling */

static void
profile_do_start(milli)
long milli;
{
    start_prof_tim(milli);
    prof_users++;
}

static void
profile_do_stop()
{
    if (--prof_users == 0) {
	DPRINTF(1, ("stop profile timer\n"));
	stop_prof_tim();
    }
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

void profile_timer(sp)
struct stframe sp;
{
	char *prof_ptr, *prof_end;
	long *a6;


	if (sp.st_sr & SUPERVISOR) {
		if (profile_ptr == NULL) {
			/* This can happen during user profiling */
			return;
		}

		prof_ptr= profile_ptr;
		prof_end= profile_tail;
		prof_ptr= buf_put_uint32(prof_ptr, prof_end, 
			sp.st_pc_low | (sp.st_pc_high << 16));
		a6 = geta6();
		do
		{
			a6 = * (long **) a6;
			if (!probe((vir_bytes) a6, 4))
				break;
			prof_ptr= buf_put_uint32(prof_ptr, prof_end, a6[1]);
		} while (* (long **) a6 > a6);

		prof_ptr= buf_put_uint32(prof_ptr, prof_end, 0L);
		if (!prof_ptr)
		{
			(*profile_done)(profile_ptr-profile_head);	
			/* ignore the last trace. */
			return;
		}
		profile_ptr= prof_ptr;
	} else {
		user_prof_tick(sp.st_pc_low | (sp.st_pc_high << 16));
	}
}

errstat profile_start(prof_buffer, prof_size, milli, prof_done)
char *prof_buffer;
bufsize prof_size;
long milli;
void (*prof_done) _ARGS(( int /* bufsize */ res ));
{
	printf("profile_start(0x%x, %d, %d, 0x%x) called\n", prof_buffer,
		prof_size, milli, prof_done);
	profile_head= profile_ptr= prof_buffer;
	profile_tail= prof_buffer+prof_size;
	profile_done= prof_done;
	profile_do_start(milli);
	return STD_OK;
}

void profile_stop()
{
	profile_do_stop();
}

#endif /* PROFILE */

void stacktrace(){
  register long *a6 = geta6();

  printf("stack trace: ");
  do {
	a6 = * (long **) a6;
	if (!probe((vir_bytes) a6, 4)) {
		printf(" ???");
		break;
	}
	printf(" %lx", a6[1]);
  } while (* (long **) a6 > a6);
  printf("\n");
}

#ifndef NOPROC

static int
sigreturn(us)
    thread_ustate *us;
{
    thread_ustate *userframe;
    struct fault *tp;

    /*
     * The copy of A1 in *us points to a frame that the users wants to RTE
     * to. We copy this frame to *us and return. The rest of the work is done 
     * by swtrap.
     *
     * We copy a whole thread_ustate, see above.
     */

    tp = USTATE_FAULT_FRAME(us);
    userframe = (thread_ustate *) umap(curthread,
					(vir_bytes) tp->f_regs[REG_A1],
					(vir_bytes) sizeof(thread_ustate), 0);
    if (userframe == 0)
	return 0;
    *us = *userframe;
    return 1;
}

/*ARGSUSED*/
void
swtrap(signo, reason, framepp)
    signum signo;
    int reason;		/* unused */
    thread_ustate **framepp;
{
    thread_ustate copyframe;
    thread_ustate *tp = *framepp;
    char *top;
    int	all_is_well;
    int size;

    /*
     * First compute end of current fault frame
     */
    top = (char *)tp+USTATE_SIZE(tp);

    copyframe = **framepp;
    if (thread_sigpend & TDS_SRET) {
	thread_sigpend &= ~TDS_SRET;
	all_is_well = sigreturn(&copyframe);
    } else {
	DPRINTF(1, ("Usertrap %d, sigpend=%x\n", signo, thread_sigpend));
	all_is_well = usertrap(signo, &copyframe);
    }
    if (all_is_well) {
	/*
	 * Pushing a new fault frame.
	 * Since it is not necessarily the same size as the one we
	 * had, space was reserved in as.S, which we may use here.
	 * So calculate new kernel sp here.
	 *
	 * Furthermore, this is a user supplied frame, so we should
	 * actually do some consistency checks here.
	 * Somebody might look at this sometime.
	 */
	size = USTATE_SIZE(&copyframe);
	tp = (thread_ustate *) (top - size);
	/*
	 * If the frame we have is too big it would overrun our stack.
	 * It should not touch framepp. Furthermore, if we do not have
	 * a floating point coprocessor we do not accept its frames.
	 */
	if ((uint32) tp <= (uint32) &framepp ||
				copyframe.m68us_flfault.flf_flag && !has_fp) {
		printf("Silently killing thread, because of fault frame\n");
		all_is_well = 0;
	}
    }
    if (!all_is_well)
	exitthread((long *) 0);
    /*
     * for security:
     * supervisor mode off, priority 0
     */
    USTATE_FAULT_FRAME(&copyframe)->f_sr &= ~(SUPERVISOR|PRI_MASK);
    (void) memmove((_VOIDSTAR) tp, (_VOIDSTAR) &copyframe, (size_t) size);
    *framepp = tp;
    /*
     * We need to call the scheduler now to deal with the case that an
     * infinite loop in the catcher mechanism occurs.  If the signal catcher
     * in user space returns and the problem that caused the trap will be
     * restarted - eg. bus error - then enqueued interrupts never get a chance
     * to run
     */
    enable();
    scheduler();
}

int
callcatcher(trapno, sv, us)
    long trapno;
    sig_vector *sv;
    thread_ustate *us;
{
    struct fault *frame;
    struct catchframe {
	long c_dummyretaddr;
        long c_trapno;            /* arg 1: trap number */
        long c_faddr;             /* arg 2: fault address */
        long c_arg3;              /* arg 3: user specified */
        long c_arg4;              /* arg 4: user specified */
        thread_ustate c_fault;     /* user state itself */
    } *cf;

    frame = USTATE_FAULT_FRAME(us);
    /*
     * Increase user stack to cope with catchframe.
     * We take maximum size here. Who cares about efficiency when
     * signals are concerned.
     */
    cf = (struct catchframe *) umap(curthread,
		(vir_bytes) frame->f_regs[REG_USP]-sizeof(*cf),
		(vir_bytes) sizeof(*cf), 1);
    if (cf == 0)
	return 0;
    (void) memmove((_VOIDSTAR) &cf->c_fault, (_VOIDSTAR) us,
		   (size_t) USTATE_SIZE(us));
    cf->c_arg4 = sv->sv_arg4;
    cf->c_arg3 = sv->sv_arg3;
    cf->c_faddr = (long)frame->f_regs[REG_USP] - sizeof(thread_ustate);
    cf->c_trapno = trapno;
    cf->c_dummyretaddr = -1;

    frame->f_regs[REG_USP] = frame->f_regs[REG_USP]-sizeof(*cf);
    frame->f_pc_high = sv->sv_pc>>16;
    frame->f_pc_low = sv->sv_pc&0xFFFF;
    /*
     * The fault frame currently on the kernel stack can hold all
     * sort of internal status, to restart an instruction in the middle.
     * That is the last thing we want now, so modify the fault frame to
     * be a format 0 frame.
     */
    frame->f_offset = 0;

    DPRINTF(1, ("About to return from callcatcher, sp=%x, pc=%x\n", frame->f_regs[REG_USP], sv->sv_pc));
    return 1;
}
#endif /* NOPROC */
    
static void
printfault(reason, sp)
uint16 reason;
register struct fault *sp;
{
	static char adr[] = "   address          = %08lx\n";
	static char ref[] = "   reference        = %s %s\n";
	static char *regname[] = {
		"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
		"a0", "a1", "a2", "a3", "a4", "a5", "a6", "sp",
	};
	register i, j;

	printf("\ntrap: ");
	if ((unsigned) reason < SIZEOFTABLE(error)) 
		printf("%s\n", error[reason].e_name);
	else
		printf("%u\n", reason);
	printf("   status register  =     %04x\n", sp->f_sr);
	printf("   program counter  = %08lx\n", F_PC(sp));
	printf("   kernel stack ptr = %08lx\n", sp);
	printf("   current thread     =     %4x\n", THREADSLOT(curthread));
	printf("   format/offset    =     %4x\n", sp->f_offset);
	switch (Format(sp)) {
	case 0x0:
	case 0x1:
		break;
	case 0x2:
	case 0x9:
		printf("   instruction address  = %08lx\n", sp->f_f2.f2_iaddr);
		break;
	case 0x8:
		printf(adr, sp->f_f8.f8_faddr);
		printf(ref, sp->f_f8.f8_ssw & F8_RW ? "read" : "write",
					space[sp->f_f8.f8_ssw & FUNCODE]);
		break;
	case 0xA:
	case 0xB:
		printf(adr, sp->f_fA.fA_faddr);
		printf(ref, sp->f_fA.fA_ssw & FA_RW ? "read" : "write",
					space[sp->f_fA.fA_ssw & FUNCODE]);
		break;
#ifdef STACKFRAME_68000
	case 0xF:
		printf(adr, sp->f_fF.fF_faddr);
		printf(ref, sp->f_fF.fF_ssw & FF_RW ? "read" : "write",
					space[sp->f_fF.fF_ssw & FUNCODE]);
		break;
#endif /* STACKFRAME_68000 */
	default:
		printf("bad format ???\n");
	}
	for (i = 0; i < 16; ) {
		for (j = 0; j < 4; i++, j++)
			printf("    %s = %08lx", regname[i], sp->f_regs[i]);
		printf("\n");
	}
}

void
trap(reason, fault)
uint16 reason;
thread_ustate **fault;
{
  register thread_ustate *sp;
  register struct fault *fp;
  static int fatal;

  if (fatal) {
	disable();
	printf("\nfatal\n");
	exitkernel();
	/*NOTREACHED*/
  }
  sp = *fault;
  fp = USTATE_FAULT_FRAME(sp);
#ifdef STACKFRAME_68000
  if (Format(fp) == 0xF)
	fp->f_fF = errorframe;
#endif /* STACKFRAME_68000 */
  if (reason == 0)
	reason = Offset(fp) >> 2;
  if (fp->f_sr & SUPERVISOR) {	/* supervisor */
#ifndef NO_OUTPUT_ON_CONSOLE
	console_enable(1);
#endif
	if (reason == TRAPV_TRACE)		/* trace trap */
		return;
	fatal = 1;
	printfault(reason, fp);
	disable();
  }
#ifndef NOPROC
  else {	/* user trap */
	/*
	 * If it was a bus error during a RM cycle give MM code
	 * a chance to fix up things.
	 */
	if (reason == TRAPV_BUS_ERROR &&
		(Format(fp) == 0xA || Format(fp) == 0xB) &&
			(fp->f_fA.fA_ssw & FA_RM) &&
			mmrerun((char *) fp->f_fA.fA_faddr,
				    (int) (fp->f_fA.fA_ssw & FUNCODE)))
		return;		/* rerun instruction */
#ifdef PMMUDEBUG
	switch (Format(fp)) {
	case 0xA:
	case 0xB:
		pmmudebug(fp->f_fA.fA_faddr);
		break;
	}
#endif /* PMMUDEBUG */
	if (reason < SIZEOFTABLE(error)) {
		putsig(curthread, error[reason].e_sig);
		return;
	}
	fatal = 1;
  }
#endif /* NOPROC */
  stacktrace();
  printf("fatal\n");
  exitkernel();
  /*NOTREACHED*/
}

static struct spurinfo {
	unsigned long spi_time;
	long spi_off;
	long spi_cnt;
} spurinfo;

/*
 * NB. You can't call getmilli() here since the hardware version requires
 * interrupts to be enabled and here they are definitely disabled.
 */
extern unsigned long milli_uptime;

static void
spurious(stframe)
struct stframe stframe;
{
  if (spurinfo.spi_off == stframe.st_vecoff &&
				spurinfo.spi_time - milli_uptime < 15000)
		spurinfo.spi_cnt++;
  else {
	if (spurinfo.spi_cnt)
		printf("%ld spurious interrupts\n",spurinfo.spi_cnt);
	spurinfo.spi_time = milli_uptime;
	spurinfo.spi_off = stframe.st_vecoff;
	spurinfo.spi_cnt = 0;
	printf("spurious interrupt at vector %ld(offset %ld)",
				stframe.st_vecoff>>2, stframe.st_vecoff);
	printf(", pc = %lx, sr = %x\n", 
		stframe.st_pc_low | (stframe.st_pc_high << 16), stframe.st_sr);
  }
}

/*
 * Catch interrupt at vector vecno in routine rout
 * rout is called with one argument, also returned by setvec
 * Current implementation is vector offset, ie 4*vectornumber
 */

int
setvec(vecno, rout)
int vecno;
void (*rout) _ARGS(( int ));
{
  intswitch[vecno] = rout;
  return 4*vecno;
}

/*
 * Call rout with argument arg, storing result in *result, but expecting
 * a trap through vector. Useful for probe, checking for coprocessors etc..
 */
static int
try_and_catch(rout, arg, result, vector)
long (*rout) _ARGS(( vir_bytes addr ));
long arg;
long *result;
int vector;
{
  char *savevec;
  int trapped;

  savevec = *ABSPTR(char **, (phys_bytes) IVECBASE+(vector<<2));
  *ABSPTR(char **, (phys_bytes) IVECBASE+(vector<<2)) = (char *) trap_to_here;
  trapped = call_with_trap(rout, arg, result);
  *ABSPTR(char **, (phys_bytes) IVECBASE+(vector<<2)) = savevec;
  return trapped;
}

static
long probestub(addr)
vir_bytes addr;
{
  char dummy;

  dummy = *ABSPTR(char *, addr);
  return dummy;
}

/* ARGSUSED */
int probe(addr, width)
vir_bytes addr;
int width; /* unused */
{
  long result;

  if (try_and_catch(probestub, (long) addr, &result, TRAPV_BUS_ERROR)) {
	return 0;
  }
  return 1;
}

static
long wprobestub(addr)
vir_bytes addr;
{

  *ABSPTR(char *, addr) = 0;
  return 0L;
}

/* ARGSUSED */
int wprobe(addr, width)
vir_bytes addr;
int width; /* unused */
{
  long result;

  if (try_and_catch(wprobestub, (long) addr, &result, TRAPV_BUS_ERROR))
	return 0;
  return 1;
}

void
inittrap(){
  register struct vector *v;
  register i;
  extern char vectab[], nextvec[];
  long result;

  for (i = 0; i < NVEC; i++) {
	(void) setvec(i, i < 16 ? exception : (void (*)()) spurious);
	*ABSPTR(char **, (phys_bytes) (i << 2) + IVECBASE) =
					&vectab[i * (nextvec - vectab)];
  }
  for (v = vectors; v->v_vector != 0; v++)
	if (v->v_catch == 0)
		(void) setvec(v->v_vector, exception);
	else
		*ABSPTR(void (**)(), (phys_bytes) (v->v_vector<<2) + IVECBASE) =
					v->v_catch;
#ifdef STACKFRAME_68000
  if (is68000) {
	*ABSPTR(void (**)(), IVECBASE+0x8) = be_68000;
	*ABSPTR(void (**)(), IVECBASE+0xC) = ae_68000;
  }
#endif /* STACKFRAME_68000 */
  if (try_and_catch(move_tt1, 0L, &result, TRAPV_LINE_F)==0)
	is_mc68030++;
  if (try_and_catch(fnop, 0L, &result, TRAPV_LINE_F)==0)
	has_fp++;
}
