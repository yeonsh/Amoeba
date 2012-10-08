/*	@(#)interrupt.S	1.13	96/02/27 13:45:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * interrupt.S
 *
 * Interrupt processing. I.e. processor traps, device interrupts,
 * and user system calls.
 */
#include <assyntax.h>
#include <trap.h>
#include <i80386.h>
#include <asdebug.h>

	AS_BEGIN

	SEG_TEXT

/*
 * Size of ``the'' Amoeba header
 */
#ifdef USE_AM6_RPC
#define	HDRSIZE		64		/* Amoeba 6.0 */
#else
#define	HDRSIZE		32		/* Amoeba 5.0 */
#endif

/*
 * flags register settings
 */
#define	FLAGS_OFF	0x002		/* all flags off */
#define	FLAGS_INTRON	0x202		/* all flags off except interrupts */

/*
 * Save/restore registers pushes/pops 12 registers (8+4)
 */
#define SAVE_REGISTERS \
	PUSHA_L		;\
	NOP		;\
	PUSH_SR	(DS)	;\
	PUSH_SR	(ES)	;\
	PUSH_SR	(FS)	;\
	PUSH_SR	(GS)
#define	RESTORE_REGISTERS \
	POP_SR	(GS)	;\
	POP_SR	(FS)	;\
	POP_SR	(ES)	;\
	POP_SR	(DS)	;\
	POPA_L		;\
	NOP

/*
 * Useful offsets into a fault frame
 */
#define	TNUM		52		/* trap number */
#define	INT_CS		64		/* cs register */

/*
 * Save/restore context (memory map) for users
 */
#define SAVE_CONTEXT \
	MOV_L	(CR3,EAX)	;\
	PUSH_L	(EAX)

#define RESTORE_CONTEXT \
	POP_L	(ECX)

/*
 * Save the kernel stack pointer of the user that is getting
 * started in the tss so it will be used on the next transition
 * to the kernel. The parameter says how much to add to sp
 * before storing it in the tss.
 */
#define SAVE_KSP(arg) \
	CLI				;\
	MOV_L	(ESP,EBX)		;\
	ADD_L	(arg,EBX)		;\
	MOV_L	(ADDR(GLNAME(tss)),ECX)	;\
	MOV_L	(EBX,REGOFF(4,ECX)) 

/* 
 * Processor traps
 */

/* division by zero */
	GLOBL	GLNAME(div0trap)
GLNAME(div0trap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_DIVIDE))
	JMP	(common_trap)

/* debug exceptions */
	GLOBL	GLNAME(dbgtrap)
GLNAME(dbgtrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_DEBUG))
	JMP	(common_trap)

/* non maskable interrupt */
	GLOBL	GLNAME(nmiint)
GLNAME(nmiint):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_NMI))
	JMP	(common_trap)

/* breakpoint */
	GLOBL	GLNAME(brktrap)
GLNAME(brktrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_BREAKPOINT))
	JMP	(common_trap)

/* overflow */
	GLOBL	GLNAME(ovflotrap)
GLNAME(ovflotrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_OVERFLOW))
	JMP	(common_trap)

/* bounds check */
	GLOBL	GLNAME(boundstrap)
GLNAME(boundstrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_BOUNDS))
	JMP	(common_trap)

/* invalid opcode */
	GLOBL	GLNAME(invoptrap)
GLNAME(invoptrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_INVALIDOP))
	JMP	(common_trap)

/* device (coprocessor) not available */
	GLOBL	GLNAME(ndptrap)
GLNAME(ndptrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_NDPNA))
	JMP	(common_trap)

/* double fault */
	GLOBL	GLNAME(dblfault)
GLNAME(dblfault):
	/* already have error code on stack */
	PUSH_L	(CONST(TRAP_DOUBLEFAULT))
	JMP	(common_trap)

/* coprocessor segment overrun */
	GLOBL	GLNAME(overrun)
GLNAME(overrun):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_OVERRUN))
	JMP	(common_trap)

/* invalid TSS */
	GLOBL	GLNAME(invtsstrap)
GLNAME(invtsstrap):
	/* already have error code on stack */
	PUSH_L	(CONST(TRAP_INVALIDTSS))
	JMP	(common_trap)

/* segment not present */
	GLOBL	GLNAME(segnptrap)
GLNAME(segnptrap):
	/* already have error code on stack */
	PUSH_L	(CONST(TRAP_SEGNOTPRESENT))
	JMP	(common_trap)

/* stack fault */
	GLOBL	GLNAME(stktrap)
GLNAME(stktrap):
	/* already have error code on stack */
	PUSH_L	(CONST(TRAP_STACKFAULT))
	JMP	(common_trap)

/* general protection */
	GLOBL	GLNAME(gptrap)
GLNAME(gptrap):
	/* already have error code on stack */
	PUSH_L	(CONST(TRAP_GENPROT))
	JMP	(common_trap)

/* page fault */
	GLOBL	GLNAME(pftrap)
GLNAME(pftrap):
	/* already have error code on stack */
	PUSH_L	(CONST(TRAP_PAGEFAULT))
	JMP	(common_trap)

/* coprocessor error */
	GLOBL	GLNAME(ndperr)
GLNAME(ndperr):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_NDPERROR))
	JMP	(common_trap)

/* alignment check (486 only) */
	GLOBL	GLNAME(aligntrap)
GLNAME(aligntrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_ALIGNMENT))
	JMP	(common_trap)

/* general invalid trap */
	GLOBL	GLNAME(invaltrap)
GLNAME(invaltrap):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(TRAP_INVALIDTRAP))
	JMP	(common_trap)

/*
 * Hardware interrupts
 */

/* interrupt request vector 0 */
	GLOBL	GLNAME(irq_0_vect)
GLNAME(irq_0_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(0))
	JMP	(common_interrupt)

/* interrupt request vector 1 */
	GLOBL	GLNAME(irq_1_vect)
GLNAME(irq_1_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(1))
	JMP	(common_interrupt)

/* interrupt request vector 2 */
	GLOBL	GLNAME(irq_2_vect)
GLNAME(irq_2_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(2))
	JMP	(common_interrupt)

/* interrupt request vector 3 */
	GLOBL	GLNAME(irq_3_vect)
GLNAME(irq_3_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(3))
	JMP	(common_interrupt)

/* interrupt request vector 4 */
	GLOBL	GLNAME(irq_4_vect)
GLNAME(irq_4_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(4))
	JMP	(common_interrupt)

/* interrupt request vector 5 */
	GLOBL	GLNAME(irq_5_vect)
_irq_5_vect:
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(5))
	JMP	(common_interrupt)

/* interrupt request vector 6 */
	GLOBL	GLNAME(irq_6_vect)
GLNAME(irq_6_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(6))
	JMP	(common_interrupt)

/* interrupt request vector 7 */
	GLOBL	GLNAME(irq_7_vect)
GLNAME(irq_7_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(7))
	JMP	(common_interrupt)

/* interrupt request vector 8 */
	GLOBL	GLNAME(irq_8_vect)
GLNAME(irq_8_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(8))
	JMP	(common_interrupt)

/* interrupt request vector 9 */
	GLOBL	GLNAME(irq_9_vect)
GLNAME(irq_9_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(9))
	JMP	(common_interrupt)

/* interrupt request vector 10 */
	GLOBL	GLNAME(irq_10_vect)
GLNAME(irq_10_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(10))
	JMP	(common_interrupt)

/* interrupt request vector 11 */
	GLOBL	GLNAME(irq_11_vect)
GLNAME(irq_11_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(11))
	JMP	(common_interrupt)

/* interrupt request vector 12 */
	GLOBL	GLNAME(irq_12_vect)
GLNAME(irq_12_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(12))
	JMP	(common_interrupt)

/* interrupt request vector 13 */
	GLOBL	GLNAME(irq_13_vect)
GLNAME(irq_13_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(13))
	JMP	(common_interrupt)

/* interrupt request vector 14 */
	GLOBL	GLNAME(irq_14_vect)
GLNAME(irq_14_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(14))
	JMP	(common_interrupt)

/* interrupt request vector 15 */
	GLOBL	GLNAME(irq_15_vect)
GLNAME(irq_15_vect):
	PUSH_L	(CONST(0))
	PUSH_L	(CONST(15))
	JMP	(common_interrupt)

/*
 * Common entry point for trap. For stack layout, see fault.h.
 * When common_trap gets called, the error code and trap number
 * have been pushed.
 */
	GLOBL	common_trap
common_trap:
	PUSH_L	(CONST(FLAGS_OFF))	/* disable further interrupts */
	POPF_L
	SAVE_REGISTERS			/* save registers */
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)			/* setup segment registers */
	MOV_SR	(AX,ES)
	XOR_L	(EBP,EBP)		/* end chain for stacktrace */
	MOV_L	(ESP,ESI)
	PUSH_L	(ESI)			/* fault frame */
	PUSH_L	(REGOFF(TNUM,ESI))	/* trap number (reason) */
	CALL	(GLNAME(trap))		/* trap(reason, frame) */
	/* do not pop arguments (see swtrap below) */
common_trap1:
	OR_L	(EAX,EAX)		/* stop? */
	JZ	(suicide)		/* yes */
#ifndef NOPROC
	/* have we got yet another exception? */
	MOV_L	(CONTENT(GLNAME(thread_sigpend)),EAX)
	OR_L	(EAX,EAX)
	JZ	(common_uexit)		/* trap again if so */
	STI
	PUSH_L	(CONTENT(GLNAME(thread_sig)))
	CALL	(GLNAME(swtrap))	/* swtrap(thread_sig, reason, frame) */
	ADD_L	(CONST(4),ESP)
	JMP	(common_trap1)		/* see if there are any more */
#else
	JMP	(common_exit)
#endif /* NOPROC */

/*
 * Return from interrupt or trap.
 *
 * stack:
 *	-------------------- <- Save this as kernel stack pointer (!NOPROC).
 *	| stuff for iret:  |
 *	| ss, sp (if user) |
 *	| flags, cs, pc	   |
 *      --------------------
 *	| number, errcode  |
 *	--------------------
 *	|   registers: 12  |
 *	--------------------
 *	|     old cr3      |
 *	--------------------
 *	|   frame ptr      |
 *	--------------------
 *	|    reason        |
 * sp ->--------------------
 */
common_uexit:
	SAVE_KSP(CONST(88))
common_exit:				/* common exit for trap and int */
	ADD_L	(CONST(8),ESP)		/* toss reason and frame ptr */
common_iexit:				/* special exit for common_interrupt */
	RESTORE_CONTEXT			/* restore context */
	RESTORE_REGISTERS		/* restore registers */
	ADD_L	(CONST(8),ESP)		/* toss errcode and number */
	IRET

/*
 * Handle hardware interrupts
 */
	GLOBL	common_interrupt
common_interrupt:
	PUSH_L	(CONST(FLAGS_OFF))	/* disable further interrupts */
	POPF_L
	SAVE_REGISTERS			/* save registers */
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX) /* setup segment registers */
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

/* acknowledge interrupt for this IRQ level */
	MOV_L	(ESP,ESI)
	MOV_L	(REGOFF(TNUM,ESI),EBX)	/* IRQ level */
	CALL	(pic_intr)
	JC	(common_iexit)		/* false alarm */

/* call the interrupt routine */
	PUSH_L	(ESI)			/* fault frame */
	MOV_L	(REGOFF(TNUM,ESI),EDI)
	PUSH_L	(EDI)			/* trap number */
#ifdef STATISTICS
	MOV_L	(ADDR(GLNAME(icount)),EBX)
	INC_L	(REGBISD(EBX,EDI,4,0))
#endif
	MOV_L	(ADDR(GLNAME(ivect)),EBX)
	CALL	(CODEPTR(REGBISD(EBX,EDI,4,0)))	/* (*_ivect[number])() */
	/* do not pop arguments (see swtrap below) */

#ifndef NOPROC
/* call the scheduler when appropriate */
	MOV_L	(REGOFF(INT_CS,ESI),EAX)
	TEST_L	(CONST(0x03),EAX)	/* check if from CPL != 0 (user) */
	JZ	(common_exit)		/* if kernel mode, just return */
	MOV_L	(CONTENT(GLNAME(schedlevel)),EAX)
	OR_L	(EAX,EAX)		/* schedule if necessary */
	JZ	(common_uexit)
	STI
	CALL	(GLNAME(scheduler))

/* call signal handler when there's a pending signal */
	MOV_L	(CONTENT(GLNAME(thread_sigpend)),EAX)
	OR_L	(EAX,EAX)
	JZ	(common_uexit)
	STI
	PUSH_L	(CONTENT(GLNAME(thread_sig)))
	CALL	(GLNAME(swtrap))	/* swtrap(thread_sig, reason, frame) */
	ADD_L	(CONST(4),ESP)
	JMP	(common_trap1)
#else
	JMP	(common_exit)
#endif /* NOPROC */

#ifndef NOPROC
/*
 * System call interface.
 *
 * We pass all of the parameters as much as possible in registers.
 */

/* 
 * All system calls return through here
 */
syscall_return:
	MOV_L	(CONTENT(GLNAME(thread_sigpend)),EBX)	/* determine whether */
	OR_L	(EBX,EBX)			/*  we have a pending signal */
	JZ	(syscall_exit)
	
/*
 * Have a pending signal, need to push a fault frame in preparation for
 * calling swtrap. The library stub already saved all registers so push
 * -1 here when don't care.
 * Right now stack looks like this: 
 *   (look in fault.h to see what it will look like)
 *	--------------------
 *	| iret stuff: ss,  |
 *	| sp, iflags, cs   |      
 *	| and pc           |
 *	--------------------
 *	|   old cr3        | 
 * sp ->--------------------
 *	
 * Use cr3 as error code as swtrap doesn't care anyway.
 */
	PUSH_L	(CONST(-1))		/* garbage trap type */
	PUSH_L	(EAX)			/* ax */
	PUSH_L	(CONST(-1))		/* cx */
	PUSH_L	(EDX)			/* dx - might be new fault frame */
	PUSH_L	(CONST(-1))		/* bx */
	PUSH_L	(CONST(-1))		/* sp  */
	PUSH_L	(EBP)			/* try to save at least bp */
	PUSH_L	(CONST(-1))		/* si */
	PUSH_L	(CONST(-1))		/* di */
	PUSH_L	(CONST(U_DS_SELECTOR))	/* ds */
	PUSH_L	(CONST(U_DS_SELECTOR))	/* es */
	PUSH_L	(CONST(U_DS_SELECTOR))	/* fs */
	PUSH_L	(CONST(U_DS_SELECTOR))	/* gs */
	MOV_L	(REGOFF(52,ESP),ECX)
	PUSH_L	(ECX)			/* old cr3 */
	MOV_L	(ESP,ECX)
	PUSH_L	(ECX)			/* frame pointer */
	PUSH_L	(CONST(-1))		/* reason - don't care for swtrap */
	PUSH_L	(CONTENT(GLNAME(thread_sig)))
	CALL	(GLNAME(swtrap))	/* swtrap(thread_sig, reason, frame) */
	ADD_L	(CONST(4),ESP)
	JMP	(common_trap1)		/* now that have a frame on the */
					/* stack use common_uexit */

/*
 * syscall_exit: do not have a fault frame on the stack
 */
syscall_exit:
	SAVE_KSP(CONST(24))
	RESTORE_CONTEXT			/* restore context */
	MOV_L	(CONST(U_DS_SELECTOR),EBX)
	MOV_SR	(BX,DS)
	MOV_SR	(BX,ES)
	MOV_SR	(BX,FS)
	MOV_SR	(BX,GS)
	IRET

/*
 * trans system call interface
 */
	GLOBL GLNAME(utrans)
GLNAME(utrans):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

/* push arguments to prevent overwrites by umap() */
	PUSH_L	(EBP)			/* cnt2 */
	PUSH_L	(EDX)			/* buf2 */
	PUSH_L	(EDI)			/* hdr2 */
	PUSH_L	(ECX)			/* cnt1 */
	PUSH_L	(EBX)			/* buf1 */
	PUSH_L	(ESI)			/* hdr1 */

/* mark end of stack chain */
	XOR_L	(EBP,EBP)

/* map header 1 in as non modify-able data */
	PUSH_L	(CONST(0))		/* might not modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(ESI)			/* hdr1 */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	MOV_L	(EAX,REGIND(ESP))	/* mapped in header */

/* map header 2 in as modify-able data */
	PUSH_L	(CONST(1))		/* might modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(EDI)			/* hdr2 */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	MOV_L	(EAX,REGOFF(12,ESP))	/* mapped in header */

	CALL	(GLNAME(sc_trans))
	ADD_L	(CONST(24),ESP)
	JMP	(syscall_return)

/*
 * getreq system call interface
 */
	GLOBL GLNAME(ugetreq)
GLNAME(ugetreq):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

/* push arguments to prevent overwrites by umap() */
	PUSH_L	(EBX)			/* cnt */
	PUSH_L	(ESI)			/* buf */

/* mark end of stack chain */
	XOR_L	(EBP,EBP)

/* map header in as modify-able data */
	PUSH_L	(CONST(1))		/* might modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(EDI)			/* hdr */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	PUSH_L	(EAX)			/* mapped in header */

	CALL	(GLNAME(sc_getreq))
	ADD_L	(CONST(12),ESP)
	JMP	(syscall_return)

/*
 * putrep system call interface
 */
	GLOBL GLNAME(uputrep)
GLNAME(uputrep):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

/* push arguments to prevent overwrites by umap() */
	PUSH_L	(EBX)			/* cnt */
	PUSH_L	(ECX)			/* buf */

/* mark end of stack chain */
	XOR_L	(EBP,EBP)

/* map header in as non modify-able data */
	PUSH_L	(CONST(0))		/* might not modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(ESI)			/* hdr */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	PUSH_L	(EAX)			/* mapped in header */

	CALL	(GLNAME(sc_putrep))
	ADD_L	(CONST(12),ESP)
	JMP	(syscall_return)

/*
 * timeout system call interface
 */
	GLOBL GLNAME(utimeout)
GLNAME(utimeout):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	PUSH_L	(EBX)
	XOR_L	(EBP,EBP)		/* end of static chain */
	CALL	(GLNAME(_timeout))	/* timeout(maxloctime) */
	ADD_L	(CONST(4),ESP)
	JMP	(syscall_return)

/*
 * cleanup system call interface
 */
	GLOBL GLNAME(ucleanup)
GLNAME(ucleanup):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	XOR_L	(EBP,EBP)		/* end of static chain */
#ifdef USE_AM6_RPC
	CALL	(GLNAME(rpc_cleanup))
#else
	CALL	(GLNAME(cleanup))
#endif /* USE_AM6_RPC */
	JMP	(syscall_return)

/*
 * await system call interface
 */
	GLOBL GLNAME(uawait)
GLNAME(uawait):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	XOR_L	(EBP,EBP)		/* end of static chain */
	CALL	(GLNAME(threadswitch))
	JMP	(syscall_return)

/*
 * usuicide (really: exitthread) system call interface
 */
	GLOBL GLNAME(usuicide)
GLNAME(usuicide):
	MOV_L	(EAX,EBX)		/* save parameter (EAX) in EBX */
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

	PUSH_L	(EBX)			/* the 'ready' pointer */
	XOR_L	(EBP,EBP)		/* end of static chain */
	CALL	(GLNAME(sc_exitthread))
	/*NOTREACHED*/
	CALL	(GLNAME(stacktrace))	/* just in case.. */
	HLT

/*
 * newsys system call interface
 */
	GLOBL	GLNAME(unewsys)
GLNAME(unewsys):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	PUSH_L	(EDI)			/* arg4 */
	PUSH_L	(EDX)			/* arg3 */
	PUSH_L	(ECX)			/* arg2 */
	PUSH_L	(EBX)			/* arg1 */
	MOV_L	(ESP,EAX)
	PUSH_L	(EAX)			/* pointer to args */
	PUSH_L	(ESI)			/* code */
	XOR_L	(EBP,EBP)		/* end of static chain */
	CALL	(GLNAME(newsys))	/* newsys(code, ap) */
	ADD_L	(CONST(8),ESP)		/* toss code, ap */
	POP_L	(EDX)			/* might be new fault frame (arg1) */
	ADD_L	(CONST(12),ESP)		/* pop off arguments (arg2-4) */
	JMP	(syscall_return)	

/*
 * bad system call interface
 */
	GLOBL GLNAME(ubadcall)
GLNAME(ubadcall):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	PUSH_L	(CONST(-1))
	XOR_L	(EBP,EBP)		/* end of static chain */
	CALL	(GLNAME(newsys))	/* newsys(-1) will get default action */
	ADD_L	(CONST(4),ESP)
	JMP	(syscall_return)

/*
 * rpc_trans system call interface
 */
	GLOBL GLNAME(urpc_trans)
GLNAME(urpc_trans):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

/* push arguments to prevent overwrites by umap() */
	PUSH_L	(EBP)			/* cnt2 */
	PUSH_L	(EDX)			/* buf2 */
	PUSH_L	(EDI)			/* hdr2 */
	PUSH_L	(ECX)			/* cnt1 */
	PUSH_L	(EBX)			/* buf1 */
	PUSH_L	(ESI)			/* hdr1 */

/* mark end of stack chain */
	XOR_L	(EBP,EBP)

/* map header 1 in as non modify-able data */
	PUSH_L	(CONST(0))		/* might not modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(ESI)			/* hdr1 */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	MOV_L	(EAX,REGIND(ESP))	/* mapped in header */

/* map header 2 in as modify-able data */
	PUSH_L	(CONST(1))		/* might modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(EDI)			/* hdr2 */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	MOV_L	(EAX,REGOFF(12,ESP))	/* mapped in header */

	CALL    (GLNAME(sc_rpc_trans))
	ADD_L	(CONST(24),ESP)
	JMP	(syscall_return)

/*
 * rpc_getreq system call interface
 */
	GLOBL GLNAME(urpc_getreq)
GLNAME(urpc_getreq):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

/* push arguments to prevent overwrites by umap() */
	PUSH_L	(EBX)			/* cnt */
	PUSH_L	(ESI)			/* buf */

/* mark end of stack chain */
	XOR_L	(EBP,EBP)

/* map header in as modify-able data */
	PUSH_L	(CONST(1))		/* might modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(EDI)			/* hdr */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	PUSH_L	(EAX)			/* mapped in header */

	CALL    (GLNAME(sc_rpc_getreq))
	ADD_L	(CONST(12),ESP)
	JMP	(syscall_return)

/*
 * rpc_putrep system call interface
 */
	GLOBL GLNAME(urpc_putrep)
GLNAME(urpc_putrep):
	SAVE_CONTEXT			/* save context */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)

/* push arguments to prevent overwrites by umap() */
	PUSH_L	(EBX)			/* cnt */
	PUSH_L	(ECX)			/* buf */

/* mark end of stack chain */
	XOR_L	(EBP,EBP)

/* map header in as non modify-able data */
	PUSH_L	(CONST(0))		/* might not modify */
	PUSH_L	(CONST(HDRSIZE))
	PUSH_L	(ESI)			/* hdr */
	PUSH_L	(CONTENT(GLNAME(curthread)))
	CALL	(GLNAME(umap))
	ADD_L	(CONST(16),ESP)
	PUSH_L	(EAX)			/* mapped in header */

	CALL    (GLNAME(sc_rpc_putrep))
	ADD_L	(CONST(12),ESP)
	JMP	(syscall_return)

/*
 * start(pc, sp)
 */
	GLOBL GLNAME(start)
GLNAME(start):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EBX)			/* save some registers, which */
	PUSH_L	(ECX)			/* will be restored by suicide or */
	PUSH_L	(EDX)			/* usuicide */
	PUSH_L	(EDI)
	PUSH_L	(ESI)
					/* push stuff for iret: */
	PUSH_L	(CONST(U_DS_SELECTOR))	/*   ss, sp, eflags, cs */
	PUSH_L	(REGOFF(12,EBP))
	PUSH_L	(CONST(FLAGS_INTRON))
	PUSH_L	(CONST(U_CS_SELECTOR))
	PUSH_L	(REGOFF(8,EBP))
	MOV_L	(CR3,EBX)
	PUSH_L	(EBX)			/* memory map for RESTORE_CONTEXT */

	XOR_L	(EAX,EAX)		/* clear the registers */
	XOR_L	(EBX,EBX)
	XOR_L	(ECX,ECX)
	XOR_L	(EDX,EDX)
	XOR_L	(EDI,EDI)
	XOR_L	(ESI,ESI)
	XOR_L	(EBP,EBP)
	JMP	(syscall_return)	/* check thread_sigpend & go to user */
#endif /* NOPROC */

/*
 * suicide - called if return 0 from trap() or swtrap()
 */
suicide:
	ADD_L	(CONST(8),ESP)		/* toss parameters to trap */
/* have a fault frame on the stack and then stuff from start */
	RESTORE_CONTEXT			/* restore context */
	ADD_L	(CONST(76),ESP)		/* toss rest of fault frame */
	POP_L	(ESI)			/* pop off stuff from start */
	POP_L	(EDI)
	POP_L	(EDX)
	POP_L	(ECX)
	POP_L	(EBX)
	POP_L	(EBP)
	RET
