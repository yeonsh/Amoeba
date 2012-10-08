/*	@(#)as.S	1.10	96/02/27 13:46:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "map.h"

/* the scratch registers to be saved are compiler dependent.. */
#ifdef ACK_ASSEMBLER
#define SCR_DREG	d0-d2	/* ACK also uses d2 as scratch register! */
#define SCR_AREG	a0-a1
#define SCR_REG_BYTES	20	/* bytes needed to save scratch registers */

/* SCR_SAVE_EXTRA and SCR_REST_EXTRA must be defined appropriately to save
 * and restore scratch registers other than default ones (d0,d1,a0,a1)
 * when handling system calls.
 * The default scratch registers are NOT saved and restored, by the way.
 */
#define SCR_SAVE_EXTRA	MOVL d2, AUTODEC(sp)
#define SCR_REST_EXTRA	MOVL AUTOINC(sp), d2
#else
#define SCR_DREG	d0-d1
#define SCR_AREG	a0-a1
#define SCR_REG_BYTES	16

#define SCR_SAVE_EXTRA	/* nothing extra to save */
#define SCR_REST_EXTRA	/* nothing extra to restore */
#endif

/* Floating point stuff */
/* fsave and frestore as you would like to write them:

	movl	_has_fp,sp@-
	beqs	done
	addql	#4,sp
	fsave	sp@-
	tstb	sp@
	beqs	done
	fmovem	fp0-fp7,sp@-
	fmovem	fpcr/fpsr/fpiar,sp@-
	movl	#-1,sp@-
done:

frest:
	tstb	sp@
	beqs	null
	addql	#4,sp
	fmovem	sp@+,fpcr/fpsr/fpiar
	fmovem	sp@+,fp0-fp7
null:	tstl	_has_fp
	beqs	nofp
	frestore sp@+
	bras	done
nofp:	addql	#4,sp
done:

end of how it should be done. Reality strikes here */

#ifndef ACK_ASSEMBLER
.globl	f68881_used
f68881_used:
#endif

#define FLTINIT		TSTL	GLOBAL(_has_fp); \
			WORD 0x6708; \
			MOVL	#0, AUTODEC(sp); \
			WORD 0xF35F
#define FLTSAVE		MOVL	GLOBAL(_has_fp), AUTODEC(sp); \
			WORD 0x6716;	\
			ADDQAL	#4, sp; \
			WORD	0xF327; \
			TSTB	REGINDIR(sp); \
			WORD	0x670e; \
			WORD	0xF227; WORD	0xE0FF; \
			WORD	0xF227; WORD	0xBC00; \
			MOVL	#-1, AUTODEC(sp)
#define FLTREST		TSTB	REGINDIR(sp); \
			WORD 0x670a; \
			ADDQAL	#4, sp; \
			WORD	0xF21F; WORD	0x9C00; \
			WORD	0xF21F; WORD	0xD0FF; \
			TSTL	GLOBAL(_has_fp); \
			WORD 0x6704; \
			WORD	0xF35F; \
			WORD 0x6002; \
			ADDQAL	#4,sp

AS_BEGIN

#ifdef STACKFRAME_68000
GLOBLDIR il_68000, _be_68000, _ae_68000

il_68000:				COM called from khead.S: this is a 68000
	ADDQAL	#6, sp			COM remove pc & sr
	MOVW	#1, GLOBAL(_is68000)	COM mark this is a 68000
	jmp	GLOBAL(startup)

_be_68000:
	MOVL	AUTOINC(sp), GLOBAL(_errorframe)
	MOVL	AUTOINC(sp), GLOBAL(_errorframe + 4)
	MOVW	#0xF008,REGOFFSETTED(sp,6)		COM set offset
	JRA	dotrap

_ae_68000:
	MOVL	AUTOINC(sp), GLOBAL(_errorframe)
	MOVL	AUTOINC(sp), GLOBAL(_errorframe + 4)
	MOVW	#0xF00C,REGOFFSETTED(sp,6)		COM set offset
	JRA	dotrap

#endif /* STACKFRAME_68000 */

COM trapped is called immediately after exceptions, i.e. bus errors but
COM not device generated interrupts.  It saves all the registers.  It
COM handles interrupt frames of any format.  Trapped returns to the caller,
COM which will then call "callint."

trapped:
	MOVEML	d0-d7/a0-a6,AUTODEC(sp)	COM save registers (0xFFFE)
	MOVAL	REGOFFSETTED(sp,60),a0	COM get return address in a0
	MOVAL	usp,a1			COM put user sp in its place
	MOVL	a1,REGOFFSETTED(sp,60)	COM ditto
	FLTSAVE
	MOVAL	sp,a1			COM save sp
	lea	REGOFFSETTED(sp,-400),sp COM space for long stack frames
	MOVL	a1,REGINDIR(sp)		COM pointer to exception frame
	MOVL	sp,d0			COM save sp
	MOVL	d0,AUTODEC(sp)		COM parameter to _trap
	jmp	REGINDIR(a0)		COM (eff. push exception type)

GLOBLDIR _exception

_exception:
	MOVW	#0x2000,sr		COM enable further interrupts
#ifdef STACKFRAME_68000
	TSTW	GLOBAL(_is68000)	COM 68000?
	JEQ	ex1			COM no
	MOVW	REGOFFSETTED(sp,6),REGOFFSETTED(sp,34)
					COM set offset in right place
ex1:
#endif /* STACKFRAME_68000 */
	ADDQAL	#8,sp			COM remove offset and return addr
	MOVEML	AUTOINC(sp),SCR_DREG/SCR_AREG
					COM restore registers
	ADDQAL	#4,sp			COM remove the return address
dotrap:
	JBSR	trapped
	CLRL	AUTODEC(sp)		COM push 0 as a ``reason''
	COM fall through

COM callint calls the C trap() routine with parameters "reason" and "sp".
COM "reason" will tell the C routine what has happened, while sp points to
COM the stack frame.  If trap returns, a process was interrupted.  If trap
COM returns zero, this process must be terminated: clean up the stack frame
COM and return from start().  If trap returns non-zero, the process must be
COM restarted.

callint:
	jsr	GLOBAL(_trap)		COM call trap(reason, sp)
#ifndef NOPROC
ci1:
	TSTL	GLOBAL(_thread_sigpend)	COM process signaled?
	JEQ	endtrap			COM no
	MOVW	#0x2000,sr		COM enable further interrupts
callswtrap:
	MOVL	GLOBAL(_thread_sig),AUTODEC(sp)
	jsr	GLOBAL(_swtrap)		COM swtrap(task_sig, reason, faultframe)
	ADDQAL	#4, sp
	JRA	ci1			COM and back again
endtrap:
	ADDQAL	#8,sp			COM remove parameters to trap
	MOVAL	REGINDIR(sp),sp		COM find beginning of stack frame
	FLTREST
	MOVAL	REGOFFSETTED(sp,60),a1	COM restore usp
	MOVL	a1,usp			COM ditto
	MOVEML	AUTOINC(sp),d0-d7/a0-a6	COM restore registers (0x7FFF)
	ADDQAL	#4,sp			COM remove saved usp
	rte				COM return from exception

COM The following is code that needs to be called before we return to user
COM space from a trap, interrupt or system call.  We need to see if any
COM interrupts have been enqueued.  If so they must have priority over
COM running any user thread.  Note we block out interrupts so that new ones
COM can't heppen until we get back to user space.  From there they will be
COM handled immediately.

com_rte:
	MOVW    #0x2700,sr		COM disable interrupts
	TSTL	GLOBAL(_n_enq_intr)	COM anything in interrupt queue?
	JLE	nosched
	MOVW	#0x2000,sr		COM enable further interrupts
	jsr	GLOBAL(_scheduler)	COM handle the interrupt queue
	JRA	com_rte
nosched:
	rte
#endif

COM Assembler support for try_and_catch function, call routine with a trap
COM expected. See trap.c for comments.
#ifdef GNU_ASSEMBLER
.comm	regsav, 64
#else
	SECTBSS
regsav:	SKIP 64
#endif
	SECTTEXT
GLOBLDIR _call_with_trap,_trap_to_here

_call_with_trap:
	MOVAL	REGOFFSETTED(sp,4),a0	COM function to call
	MOVL	REGOFFSETTED(sp,8),d0	COM argument
	MOVEML	d0-d7/a0-a7,GLOBAL(regsav) COM save registers in case of trap
	MOVL	d0,AUTODEC(sp)		COM push argument
	jsr	REGINDIR(a0)		COM call function
	ADDQAL	#4,sp			COM we didn't trap, pop argument
	MOVAL	REGOFFSETTED(sp,12),a0	COM pointer to result storage
	MOVL	d0,REGINDIR(a0)		COM store result
	CLRL	d0			COM return zero
	rts				COM meaning success
_trap_to_here:
	MOVEML	GLOBAL(regsav),d0-d7/a0-a7
					COM we trapped, restore regs
	MOVEQ	#-1,d0			COM return -1
	rts				COM meaning failure

COM Stub routines to test processor functionality
COM move_tt1 tests MC68030 vs MC68020
COM fnop test MC6888[12] presence

GLOBLDIR _move_tt1
_move_tt1:
	WORD	0xF02F
	WORD	0x0D00
	WORD	0x0004			COM pmove REGOFFSETTED(sp,4),tt1
	rts

GLOBLDIR _fnop
_fnop:
	WORD	0xF280
	WORD	0x0000			COM fnop
	rts

COM Below is an interrupt dispatch table of 256 jsr entries.  If the
COM processor comes here due to an interrupt, it will directly jump to
COM the routine called vector, which will then calculate the vector
COM offset using the return address, and call the corresponding C
COM interrupt handler as found in the intswitch table.  When this
COM routine returns, the processor state has to be restored.  However,
COM if processes are supported, the processor might have to be
COM rescheduled.  Also if the process received a signal it should be
COM rescheduled.

GLOBLDIR _vectab, _nextvec

#ifdef MOTOROLA
#define S       /**/ jsr vector-2-.(pc)
#else
#if defined(ACK_ASSEMBLER) || defined(GNU_ASSEMBLER)
# ifdef ACK_AS_FIXED
#  define S	jsr (vector - 2 - . , pc)	/* currently 8 bytes !!! */
# else
#  define S	LONG 0x4eba0000 + vector - 2 - .	/* 4 bytes !!! */
# endif /* ACK_AS_FIXED */
#else
#ifdef ACE_ASSEMBLER
#define	S	jsr vector-2-.(pc)
#else
#define S	jsr pc@(vector - 2 - .)			/* 4 bytes !!! */
#endif /* ACE_ASSEMBLER */
#endif /* ACK_ASSEMBLER */
#endif /* MOTOROLA */

_vectab: S
_nextvec:
	S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S
	S;S; S;S;S; S;S;S;S;S;S ;S;S;S;S;S;S;     S;S;S ;S;;S; S;S;
	S;S; S;S;S; S;S;S;S;S; S ;S;S;S;S;S; S;S;S ;S;S ;S;S ;S;S;S
	S;S; S;S;S; S;S;S;S;S ;S; S;S;S;S;S ;S;S;S;S;S; S;S ;S;S;S;
	S;S;        S;S;S;S; S;S;S ;S;S;S; ;;S;S;S;S;S;    S;S;S;S;
	S;S; S;S;S; S;S;S;S         ;S;S;S ;S;S;S;S;S;S ;S ;S;S;S;S
	S;S; S;S;S; S;S;S; S;S;S;S;S ;S;S;S ;S;S;S;S;S; S;S ;S;S;S;
	S;S; S;S;S; S;S;S ;S;S;S;S;S; S;S;S; S;S;S ;S;S ;S;S ;S;S;S
	S;S; S;S;S; S;S; S;S;S;S;S;S;S ;S;S;S     ;S;S; S;S;S ;S;S;
	S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;S;;S;S;S;S;S;S;S;

#undef S

vector:					COM save registers
	MOVW	#0x2700,sr		COM disable further interrupts
	MOVEML	SCR_DREG/SCR_AREG,AUTODEC(sp)
					COM save registers
#ifndef NDEBUG
	jsr	GLOBAL(_checkstack)
#endif
	MOVL	REGOFFSETTED(sp,SCR_REG_BYTES),d0
					COM get return address in d0
	SUBL	#_nextvec,d0		COM calculate vector offset
	MOVL	d0,AUTODEC(sp)		COM argument to interrupt routine
#ifndef NDEBUG
	MOVAL	#_intcount,a0		COM increment interrupt counter
	ADDQL	#1,BDAREGINDEXW(0,a0,d0) COM ditto
#endif
	MOVAL	#_intswitch,a0		COM get the C interrupt routine
	MOVAL	BDAREGINDEXW(0,a0,d0),a0 COM ditto
	jsr	REGINDIR(a0)		COM call the interrupt routine
	ADDQAL	#4,sp			COM remove offset parameter
#ifndef NOPROC
	TSTW	GLOBAL(_schedlevel)	COM call scheduler?
	JEQ	restore			COM no
	btst	#5,REGOFFSETTED(sp,SCR_REG_BYTES+4)
					COM only schedule in user mode
	JNE	restore			COM not user mode
	MOVAL	usp,a0			COM save user sp
	MOVL	a0,AUTODEC(sp)		COM ditto
	FLTSAVE
	MOVW	#0x2000,sr		COM enable interrupts
	jsr	GLOBAL(_scheduler)	COM process returns to user state
	MOVW	#0x2700,sr		COM disable further interrupts
	FLTREST
	MOVAL	AUTOINC(sp),a0		COM restore user sp
	MOVL	a0,usp			COM ditto
	TSTL	GLOBAL(_thread_sigpend)	COM process signaled?
	JEQ	restore			COM no
	MOVEML	AUTOINC(sp),SCR_DREG/SCR_AREG
					COM restore registers
	ADDQAL	#4,sp			COM remove the return address
dosignal:
	MOVW	#0x2000,sr		COM enable further interrupts
	CLRW	REGOFFSETTED(sp,6)	COM format/offset = 0
	JBSR	trapped			COM call the trap routine
	MOVL	#0,AUTODEC(sp)
	JRA	callswtrap
restore:				COM restore processor state
#endif
	MOVEML	AUTOINC(sp),SCR_DREG/SCR_AREG
					COM and the other registers
	ADDQAL	#4,sp			COM remove the return address
	rte				COM return from exception


#ifndef NOPROC

COM system calls

#define SWITCH			/* switch to kernel context */	\
		MOVAL usp,a0;		/* save usp */		\
		MOVL a0,AUTODEC(sp);	/* ditto */		\
		SCR_SAVE_EXTRA;		/* compiler dependent */\
		FLTSAVE;

#define RETURN			/* restore kernel context */	\
		MOVAL	AUTOINC(sp),a1;	/* restore a1 */	\
		MOVL	AUTOINC(sp),d0;	/* restore d0 */	\
		FLTREST;					\
		SCR_REST_EXTRA;		/* compiler dependent */\
		MOVAL	AUTOINC(sp),a0;	/* restore usp */	\
		MOVL	a0,usp		/* ditto */ 

COM return is called after every system call.  The transaction may be
COM interrupted half way through.  In that case the trap routine is called.

return:
	MOVL	d0,AUTODEC(sp)		COM save result
	MOVL	a1,AUTODEC(sp)		COM save part of header
	TSTL	GLOBAL(_thread_sigpend)	COM process signaled?
	JEQ	re1			COM no
	RETURN				COM restore context
	JRA	dosignal		COM handle interrupt
  re1:	RETURN				COM restore context
	rte				COM return from exception

GLOBLDIR _urpc_getreq, _urpc_trans, _urpc_putrep

_urpc_getreq:
        SWITCH
        link    a6,#-32                 COM allocate header space
        MOVAL   sp,a0                   COM pointer to allocated header
        MOVL    d3,REGINDIR(a0)         COM get port into header
        MOVW    d4,REGOFFSETTED(a0,4)
        MOVL    d1,AUTODEC(sp)          COM push cnt
        MOVL    a4,AUTODEC(sp)          COM push buf
        MOVL    a0,AUTODEC(sp)          COM push allocated header
	jsr	GLOBAL(_sc_rpc_getreq)
        lea     REGOFFSETTED(sp,12),sp  COM remove parameters
        MOVEML  REGINDIR(sp),d3-d7/a1-a3 COM copy hdr into the registers (0xEF8)
        unlk    a6                      COM free allocated space
        CLRL    d1                      COM clear the registers
        MOVAL   d1,a0                   COM ditto
        JRA     return

_urpc_putrep:
        SWITCH
        link    a6,#-32                 COM allocate header space
        MOVAL   sp,a0                   COM pointer to allocated header
        MOVEML  d3-d7/a1-a3,REGINDIR(a0) COM get hdr from (0xEF8)
        MOVL    d1,AUTODEC(sp)          COM push cnt
        MOVL    a4,AUTODEC(sp)          COM push buf
        MOVL    a0,AUTODEC(sp)          COM push allocated header
	jsr	GLOBAL(_sc_rpc_putrep)	COM call putrep, result in d0
        lea     REGOFFSETTED(sp,12),sp  COM remove parameters
        unlk    a6                      COM free allocated space
        CLRL    d1                      COM clear the registers
        MOVAL   d1,a0                   COM ditto
        MOVAL   d1,a1                   COM ditto
        JRA     return

_urpc_trans:
        SWITCH
        link    a6,#-32                 COM allocate header space
        MOVAL   sp,a0                   COM pointer to allocated header
        MOVEML  d3-d7/a1-a3,REGINDIR(a0) COM get hdr1 from (0xEF8)
        MOVL    d2,AUTODEC(sp)          COM push cnt2
        MOVL    a5,AUTODEC(sp)          COM push buf2
        MOVL    a0,AUTODEC(sp)          COM push allocated header
        MOVL    d1,AUTODEC(sp)          COM push cnt1
        MOVL    a4,AUTODEC(sp)          COM push buf1
        MOVL    a0,AUTODEC(sp)          COM push allocated header
	jsr	GLOBAL(_sc_rpc_trans)	COM call putrep, result in d0
        lea     REGOFFSETTED(sp,24),sp  COM remove parameters
        MOVEML  REGINDIR(sp),d3-d7/a1-a3 COM copy hdr into the registers (0xEF8)
        unlk    a6                      COM free allocated space
        CLRL    d1                      COM clear the registers
        MOVAL   d1,a0                   COM ditto
        JRA     return

GLOBLDIR _ugetreq, _uputrep, _utrans, _utimeout, _ucleanup
GLOBLDIR _uawait, _usuicide, _umutrylock, _umuunlock

_usuicide:
	SWITCH
	MOVL	d0,AUTODEC(sp)
	jsr	GLOBAL(_sc_exitthread)
	illegal
_ugetreq:
	SWITCH
	link	a6,#-32			COM allocate header space
	MOVAL	sp,a0			COM pointer to allocated header
	MOVL	d3,REGINDIR(a0)		COM get port into header
	MOVW	d4,REGOFFSETTED(a0,4)
	MOVL	d1,AUTODEC(sp)		COM push cnt
	MOVL	a4,AUTODEC(sp)		COM push buf
	MOVL	a0,AUTODEC(sp)		COM push allocated header
	jsr	GLOBAL(_sc_getreq)	COM do umaps and the operation
	lea	REGOFFSETTED(sp,12),sp	COM remove parameters
	MOVEML	REGINDIR(sp),d3-d7/a1-a3 COM copy hdr into the registers (0xEF8)
	unlk	a6			COM free allocated space
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	JRA	return

_uputrep:
	SWITCH
	link	a6,#-32			COM allocate header space
	MOVAL	sp,a0			COM pointer to allocated header
	MOVEML	d3-d7/a1-a3,REGINDIR(a0) COM get hdr from (0xEF8)
	MOVL	d1,AUTODEC(sp)		COM push cnt
	MOVL	a4,AUTODEC(sp)		COM push buf
	MOVL	a0,AUTODEC(sp)		COM push allocated header
	jsr	GLOBAL(_sc_putrep)	COM do umaps and the operation
	lea	REGOFFSETTED(sp,12),sp	COM remove parameters
	unlk	a6			COM free allocated space
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	MOVAL	d1,a1			COM ditto
	JRA	return

_utrans:
	SWITCH
	link	a6,#-32			COM allocate header space
	MOVAL	sp,a0			COM pointer to allocated header
	MOVEML	d3-d7/a1-a3,REGINDIR(a0) COM get hdr1 from (0xEF8)
	MOVL	d2,AUTODEC(sp)		COM push cnt2
	MOVL	a5,AUTODEC(sp)		COM push buf2
	MOVL	a0,AUTODEC(sp)		COM push allocated header
	MOVL	d1,AUTODEC(sp)		COM push cnt1
	MOVL	a4,AUTODEC(sp)		COM push buf1
	MOVL	a0,AUTODEC(sp)		COM push allocated header
	jsr	GLOBAL(_sc_trans)	COM do the umaps and call trans
	lea	REGOFFSETTED(sp,24),sp	COM remove parameters
	MOVEML	REGINDIR(sp),d3-d7/a1-a3 COM copy hdr into the registers (0xEF8)
	unlk	a6			COM free allocated space
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	JRA	return

_utimeout:
	SWITCH
	MOVL	d0,AUTODEC(sp)		COM maxloctime
#ifdef USE_AM6_RPC
	jsr	GLOBAL(_rpc_timeout)	COM call rpc_timeout
#else
	jsr	GLOBAL(__timeout)	COM call _timeout
#endif
	ADDQAL	#4,sp
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	MOVAL	d1,a1			COM ditto
	JRA	return

_ucleanup:
	SWITCH
#ifdef USE_AM6_RPC
	jsr	GLOBAL(_rpc_cleanup)	COM call cleanup
#else
	jsr	GLOBAL(_cleanup)	COM call cleanup
#endif
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	MOVAL	d1,a1			COM ditto
	JRA	return

_uawait:				COM actually only threadswitch
	SWITCH
	jsr	GLOBAL(_threadswitch)	COM call threadswitch()
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	MOVAL	d1,a1			COM ditto
	JRA	return

_umutrylock:
	SWITCH
	MOVL	d1, AUTODEC(sp)
	MOVL	d0, AUTODEC(sp)
	jsr	GLOBAL(_u_mu_trylock)
	ADDQAL	#8, sp
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	MOVAL	d1,a1			COM ditto
	JRA	return

_umuunlock:
	SWITCH
	MOVL	d0, AUTODEC(sp)
	jsr	GLOBAL(_u_mu_unlock)
	ADDQAL	#4, sp
	CLRL	d1			COM clear the registers
	MOVAL	d1,a0			COM ditto
	MOVAL	d1,a1			COM ditto
	JRA	return

GLOBLDIR	_unewsys
_unewsys:
	SWITCH
	MOVAL	usp, a0
	MOVL	a0, AUTODEC(sp)		COM usp is value result argument
	jsr	GLOBAL(_newsys)		COM and will be changed in the case
	MOVAL	AUTOINC(sp), a0		COM of sigreturn only DON'T BELIEVE IT
	MOVAL	a0, a1
	JRA	return


COM process startup

GLOBLDIR _start

_start:				COM start a user process
	MOVAL	REGOFFSETTED(sp,8),a0	COM initialize user stack pointer
	MOVL	a0,usp			COM ditto
	MOVAL	REGOFFSETTED(sp,4),a0	COM initialize user program counter
	MOVEML	d2-d7/a2-a6,AUTODEC(sp)	COM save registers (0x3F3E)
	FLTINIT
	MOVAL	#0, a6
	CLRW	AUTODEC(sp)		COM to accommodate an 68010
	MOVL	a0,AUTODEC(sp)		COM push PC
	CLRW	AUTODEC(sp)		COM push SR <<all rights reserved>>
	TSTL	GLOBAL(_thread_sigpend)
	JNE	dosignal		COM Go handle signal, if needed
	MOVEML	GLOBAL(zero),d0-d7/a0-a6 COM clear the registers (0x7FFF)
	rte				COM and start process

#endif /* NOPROC */

GLOBLDIR _geta6
_geta6:
	MOVAL	a6,a0
	MOVL	a6,d0
	rts

GLOBLDIR _getsr, _setsr

_getsr:
	MOVW	sr,d0
	rts

_setsr:
	MOVW	REGOFFSETTED(sp,6),sr
	rts


GLOBLDIR _enable, _disable, _waitint

_enable:
	MOVW	#0x2000,sr
	rts

_disable:
	MOVW	#0x2700,sr
	rts

_waitint:
	link	a6,#0			COM keep track of a6 chain
	stop	#0x2000
	unlk	a6

idle:
	rts

#if 1
GLOBLDIR _bzero
#else
GLOBLDIR _bcopy, _bzero

_bcopy:
	MOVAL	REGOFFSETTED(sp,4),a0	COM a0 = source
	MOVAL	REGOFFSETTED(sp,8),a1	COM a1 = destination
	MOVW	a1,d1			COM destination
	ANDL	#1,d1			COM odd?
	JEQ	cloop			COM no
	MOVB	AUTOINC(a0),AUTOINC(a1)	COM align destination
	SUBQL	#1,REGOFFSETTED(sp,12)	COM decrement length
cloop:
	MOVL	REGOFFSETTED(sp,12),d0	COM d0 = length
	JEQ	end			COM if length == 0 return
	CMPL	#0x7FF0,d0		COM > 7FF0 ?
	JLE	calign			COM no
	MOVL	#0x7FF0,d0		COM d0 = 7FF0
calign:
	SUBL	d0,REGOFFSETTED(sp,12)	COM length -= d0
	MOVW	a0,d1			COM source
	ANDL	#1,d1			COM odd?
	JNE	bcopy			COM yes, do byte copy
#ifdef STACKFRAME_68000
	TSTW	GLOBAL(_is68000)	COM 68000?
	JNE	lcopy			COM no, do long copy
	CMPW	#0x180,d0		COM <= 96 longs?
	JLS	lcopy			COM overhead of reg save is not worth it
	SUBW	#0xC4,d0		COM don't copy too much
	MOVEML	d1-d7/a2-a6,AUTODEC(sp)	COM save registers on stack (0x7F3E)
mcopy:
	MOVEML	AUTOINC(a0),d1-d7/a2-a6	COM copy 4 * 4 * 12 bytes (0x7CFE)
	MOVEML	d1-d7/a2-a6,REGINDIR(a1)		COM (0x7CFE)
	MOVEML	AUTOINC(a0),d1-d7/a2-a6			COM (0x7CFE)
	MOVEML	d1-d7/a2-a6,REGOFFSETTED(a1,0x30)	COM (0x7CFE)
	MOVEML	AUTOINC(a0),d1-d7/a2-a6			COM (0x7CFE)
	MOVEML	d1-d7/a2-a6,REGOFFSETTED(a1,0x60)	COM (0x7CFE)
	MOVEML	AUTOINC(a0),d1-d7/a2-a6			COM (0x7CFE)
	MOVEML	d1-d7/a2-a6,REGOFFSETTED(a1,0x90)	COM (0x7CFE)
	ADDAW	#0xC0,a1
	SUBW	#0xC0,d0		COM length -= 4 * 4 * 12
	JGE	mcopy
	MOVEML	AUTOINC(sp),d1-d7/a2-a6	COM restore registers (0x7CFE)
	ADDW	#0xC4,d0		COM adjust length
lcopy:
#endif /* STACKFRAME_68000 */
	MOVW	d0,d1
	LSRW	#2,d1			COM d1 = length >> 2
	JEQ	bcopy			COM if d1 == 0 do byte copy
	SUBQW	#1,d1
clong:
	MOVL	AUTOINC(a0),AUTOINC(a1)	COM copy per long
	dbra	d1,clong
	ANDW	#3,d0			COM length &= 3
	JEQ	cloop			COM if length == 0 do next chunk
bcopy:
	SUBQW	#1,d0
cbyte:
	MOVB	AUTOINC(a0),AUTOINC(a1)	COM copy per byte
	dbra	d0,cbyte
	JRA	cloop			COM next chunk
#endif /*1*/
end:
	rts

_bzero:
	MOVAL	REGOFFSETTED(sp,4),a1	COM a1 = destination
	MOVW	a1,d1			COM destination
	ANDL	#1,d1			COM odd?
	JEQ	zloop			COM no
	CLRB	AUTOINC(a1)		COM align destination
	SUBQL	#1,REGOFFSETTED(sp,8)	COM decrement length
zloop:
	MOVL	REGOFFSETTED(sp,8),d0	COM d0 = length
	JEQ	end			COM if length == 0 return
	CMPL	#0x7FF0,d0		COM > 7FF0 ?
	JLE	zalign			COM no
	MOVL	#0x7FF0,d0		COM d0 = 7FF0
zalign:
	SUBL	d0,REGOFFSETTED(sp,8)	COM length -= d0
#ifdef STACKFRAME_68000
	TSTW	GLOBAL(_is68000)	COM 68000?
	JNE	lzero			COM no, do long zeroing
	CMPW	#0x180,d0		COM <= 96 longs?
	JLS	lzero			COM overhead of register save not worth it
	SUBW	#0xC4,d0		COM don't zero too much
	MOVEML	d1-d7/a2-a6,AUTODEC(sp)	COM save register on stack (0x7F3E)
	MOVEML	GLOBAL(zero),d1-d7/a2-a6 COM clear the registers (0x7CFE)
mzero:
	MOVEML	d1-d7/a2-a6,REGINDIR(a1) COM zero 4 * 4 * 12 bytes (0x7CFE)
	MOVEML	d1-d7/a2-a6,REGOFFSETTED(a1,0x30)	COM (0x7CFE)
	MOVEML	d1-d7/a2-a6,REGOFFSETTED(a1,0x60)	COM (0x7CFE)
	MOVEML	d1-d7/a2-a6,REGOFFSETTED(a1,0x90)	COM (0x7CFE)
	ADDAW	#0xC0,a1
	SUBW	#0xC0,d0		COM length -= 4 * 4 * 12
	JGE	mzero
	MOVEML	AUTOINC(sp),d1-d7/a2-a6	COM restore registers (0x7CFE)
	ADDW	#0xC4,d0		COM adjust length
lzero:
#endif /* STACKFRAME_68000 */
	MOVW	d0,d1
	LSRW	#2,d1			COM d1 = length >> 2
	JEQ	bzero			COM if d1 == 0 do byte zeroing
	SUBAL	a0,a0			COM a0 = 0
	SUBQW	#1,d1
zlong:
	MOVL	a0,AUTOINC(a1)		COM zero per long
	dbra	d1,zlong
	ANDW	#3,d0			COM length &= 3
	JEQ	zloop			COM if length == 0 do next chunk
bzero:
	SUBQW	#1,d0
zbyte:
	CLRB	AUTOINC(a1)		COM zero per byte
	dbra	d0,zbyte
	JRA	zloop

#ifdef GNU_ASSEMBLER
GLOBLDIR _idletime
.comm _idletime, 4			COM USED but not set
.comm zero,	 64			COM zero region to clear regs with movem
#else
SECTBSS
GLOBLDIR _idletime
_idletime:	SKIP 4			COM USED but not set
zero:		SKIP 64			COM zero region to clear regs with movem
#endif
