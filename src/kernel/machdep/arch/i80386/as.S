/*	@(#)as.S	1.9	96/02/27 13:45:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * as.S
 *
 * Amoeba 32-bit kernel start off, and miscellaneous assembler support routines.
 *
 * Hacker Alert:
 *	The support routines all assume that parameters are passed on the
 *	stack and that they can overwrite the AX register without consequences.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Gregory J. Sharp, Oct 1992 - converted to new assyntax.h form.
 *	Gregory J. Sharp, Feb 1994 - sometimes a second floppy is not stopped
 *				     by the bios after booting so we do it.
 */
#include <assyntax.h>
#include <i80386.h>
#include <asdebug.h>
#include <ndp.h>

#define	K32_STACK_SIZE	8192	/* size of primary (32-bit) kernel stack */

/*
 * Amoeba kernel start off.
 *
 * Here is where the 32-bit mode Amoeba kernel starts. The kernel start off
 * in mmu/khead.S only warps the kernel into 32-bit mode and jumps directly
 * to kstart. It exists to perform the machine dependent functions that are
 * needed during the switch.  The kernel start off initializes the IDT
 * (interrupt descriptor table) with all the interrupt gates found in the
 * temporary idt table. To save space, this temporary table is also used to
 * store the real table. Having initialized the IDT table, it continues by
 * building a GDT table. This is where all the segment descriptors are stored.
 * Since the kernel doesn't use the standard 80386 multi tasking facilities
 * only a small number of these descriptors are needed. Those that are, are
 * the kernel DS, kernel CS, user DS, user CS, and TSS descriptors. After
 * initializing all the essential tables they are activated by placing their
 * pointers into the designated registers. After this final step all the BSS
 * section is cleared (actually only those part between ``begin'' and ``end''),
 * and control is transfered to main().
 */
	AS_BEGIN

	SEG_TEXT

	GLOBL GLNAME(kstart)
GLNAME(kstart):
	CLI

/* setup temporary stack while setting everything up */
	MOV_L	(ADDR(GLNAME(kst_end)),ESP)

/* build interrupt gate descriptors in IDT table */
	MOV_L	(ADDR(GLNAME(idt)),EBX)
	MOV_L	(CONST(IDT_SIZE),ECX)
kstart1:
	XOR_L	(EAX,EAX)
	MOV_L	(REGOFF(0,EBX),EAX)			/* offset */
	MOV_B	(REGOFF(7,EBX),DL)			/* type/access */
	MOV_W	(AX,REGOFF(0,EBX))			/* offset (low word) */
	SHR_L	(CONST(16),EAX)
	MOV_W	(AX,REGOFF(6,EBX))			/* offset (high word) */
	MOV_W	(CONST(K_CS_SELECTOR),REGOFF(2,EBX))	/* selector */
	MOV_B	(DL,REGOFF(5,EBX))			/* type/access */
	MOV_B	(CONST(0),REGOFF(4,EBX))
	ADD_L	(CONST(8),EBX)
	LOOP	(kstart1)

/* build global descriptor table */
	MOV_L	(ADDR(GLNAME(gdt)),EBX)

/* build kernel null descriptor */
	MOV_L	(CONST(0),REGOFF(NULL_SELECTOR+0,EBX))
	MOV_L	(CONST(0),REGOFF(NULL_SELECTOR+4,EBX))

/* build kernel DS segment descriptor */
	XOR_L	(EAX,EAX)
	MOV_L	(CONST(SEG_SIZE),ECX)
	MOV_B	(CONST(DATA_GRAN),DL)
	MOV_B	(CONST(KDATA_ACC),DH)
	LEA_L	(REGOFF(K_DS_SELECTOR,EBX),ESI)
	CALL	(init_descriptor)

/* build user DS segment descriptor */
	MOV_B	(CONST(UDATA_ACC),DH)
	LEA_L	(REGOFF(U_DS_SEL,EBX),ESI)
	CALL	(init_descriptor)

/* build kernel CS segment descriptor */
	MOV_B	(CONST(CODE_GRAN),DL)
	MOV_B	(CONST(KCODE_ACC),DH)
	LEA_L	(REGOFF(K_CS_SELECTOR,EBX),ESI)
	CALL	(init_descriptor)

/* build user CS segment descriptor */
	MOV_B	(CONST(UCODE_ACC),DH)
	LEA_L	(REGOFF(U_CS_SEL,EBX),ESI)
	CALL	(init_descriptor)

/* build TSS segment descriptor */
	MOV_L	(ADDR(GLNAME(tss)),EAX)
	MOV_L	(CONST(TSS_SIZE),ECX)
	MOV_B	(CONST(TSS_GRAN),DL)
	MOV_B	(CONST(TSS_ACC),DH)
	LEA_L	(REGOFF(TSS_SELECTOR,EBX),ESI)
	CALL	(init_descriptor)

/* (re)load GDTR and IDTR registers */
	LIDT	(CONTENT(idtptr))
	LGDT	(CONTENT(gdtptr))

	SEG_DATA
idtptr:	D_WORD	EMUL(IDT_SIZE,DESC_SIZE)-1
	D_LONG	GLNAME(idt)
gdtptr:	D_WORD	EMUL(GDT_SIZE,DESC_SIZE)-1
	D_LONG	GLNAME(gdt)
	SEG_TEXT

/* reload segment registers and stack pointer */
	MOV_L	(CONST(K_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	MOV_SR	(AX,FS)
	MOV_SR	(AX,GS)
	MOV_SR	(AX,SS)
	MOV_L	(ADDR(GLNAME(kst_end)),ESP)

/* reload code segment register */
	JMPF	(CONST(K_CS_SELECTOR),ADDR(kstart3))

/* build main task state segment (TSS) */
kstart3:
	MOV_L	(ADDR(GLNAME(tss)),EBX)
	MOV_L	(CONST(0),REGOFF(0,EBX))		/* tss.backlink */
	MOV_L	(ADDR(GLNAME(kst_end)),REGOFF(4,EBX))	/* tss.sp0 */
	MOV_L	(CONST(K_DS_SELECTOR),REGOFF(8,EBX))	/* tss.ss0 */

/* build I/O bitmap for IOP server */
	MOV_L	(CONST(SIZEOF_TSS),REGOFF(102,EBX))	/* tss.ioaddr */
	MOV_B	(CONST(0xFF),AL)
	LEA_L	(REGOFF(104,EBX),EDI)
	MOV_L	(CONST(TSS_SIZE-SIZEOF_TSS),ECX)
	REP	STOS_B					/* inhibit all I/O */

/* load task register */
	MOV_L	(CONST(TSS_SELECTOR),EAX)
	LTR	(AX)

/* mask unnecessary bits out of CR0 */
	MOV_L	(CR0,EAX)
	AND_L	(CONST(ENOT(EXPR(CR0_PG|CR0_ET|CR0_EM|CR0_MP|CR0_TS))),EAX)
	MOV_L	(EAX,CR0)

/* determine the kind of CPU we have (assume 386) */
	MOV_L	(CONST(CPU_386),CONTENT(GLNAME(cpu_type)))
	PUSHF_L						/* push eflags */
	POP_L	(EAX)
	MOV_L	(EAX,EBX)				/* save old settings */
	OR_L	(CONST(EFL_AC),EAX)			/* toggle AC bit */
	PUSH_L	(EAX)
	POPF_L						/* set flags */
	PUSHF_L
	POP_L	(EAX)
	PUSH_L	(EBX)					/* restore old flags */
	POPF_L
	TEST_L	(CONST(EFL_AC),EAX)
	JZ	(kstart4)
	MOV_L	(CONST(CPU_486),CONTENT(GLNAME(cpu_type)))

/* check for presence of numeric co-processor */
kstart4:
	FNINIT
	MOV_L	(CONST(ENOT(0)),EAX)
	FNSTSW	(AX)
	OR_B	(AL,AL)
	JZ	(kstart5)			/* chip present? */
	MOV_L	(CONST(NDP_NONE),CONTENT(GLNAME(ndp_type)))
	JMP	(kstart6)

/* we have a co-processor, we always assume 80387 for now */
kstart5:
	MOV_L	(CONST(NDP_387),CONTENT(GLNAME(ndp_type)))

/* turn the ndp off now we know its there */
kstart6:
	MOV_L	(CR0,EAX)
	OR_L	(CONST(CR0_EM),EAX)
	MOV_L	(EAX,CR0)

/*
 * Make sure the floppy drive motors are all off too.  Sometimes they
 * Don't stop before we have interfered with their timeouts if we don't
 * do this.
 */
	MOV_L	(CONST(0x3F2), EDX)
	MOV_L	(CONST(0xC), EAX)
	OUT_B

/* clear BSS section before calling main */
	CALL	(GLNAME(clearbss))
	XOR_L	(EBP,EBP)			/* end chain for stacktrace */
	CALL	(GLNAME(main))
	/* NOTREACHED */

/* exit and .stop functions used by ACK (note the fall through) */
	GLOBL exit, .stop, EXIT
exit:
.stop:
EXIT:
	CALL	(GLNAME(reboot))
	JMP	(exit)

/*
 * NDP co-processor support routines
 */


/*
 * Switch ndp register. Note that the registers aren't actually saved and
 * restored. This will only happen when two (or more) competing threads
 * are using the co-processor. The following algorithm is used:
 *
 *     if (ndpthread != (struct thread *) 0) {
 *	   int cr0, cr0prime;
 *	   cr0 = cr0prime = getCR0();
 *	   if (curthread == ndpthread)
 *	       cr0prime &= ~CR0_TS;
 *	   else 
 *	       cr0prime |= CR0_TS;
 *	   if (cr != cr0prime)
 *	       setCR0(cr0prime);
 *     }
 */
	GLOBL	GLNAME(ndp_switchthread)
GLNAME(ndp_switchthread):
	MOV_L	(CONTENT(GLNAME(ndpthread)),EAX)
	OR_L	(EAX,EAX)
	JZ	(ndp_switchthread2)
	MOV_L	(CR0,EBX)
	MOV_L	(EBX,ECX)
	CMP_L	(CONTENT(GLNAME(curthread)),EAX)
	JNE	(ndp_switchthread0)
	AND_L	(CONST(ENOT(CR0_TS)),ECX)
	JMP	(ndp_switchthread1)
ndp_switchthread0:
	OR_L	(CONST(CR0_TS),ECX)
ndp_switchthread1:
	CMP_L	(ECX,EBX)
	JE	(ndp_switchthread2)
	MOV_L	(ECX,CR0)
ndp_switchthread2:
	RET

/* save current co-processor state */
	GLOBL	GLNAME(ndp_save)
GLNAME(ndp_save):
	MOV_L	(REGOFF(4,ESP),EAX)
	FNSAVE	(REGIND(EAX))
	RET

/* restore co-processor state */
	GLOBL	GLNAME(ndp_restore)
GLNAME(ndp_restore):
	MOV_L	(REGOFF(4,ESP),EAX)
	FRSTOR	(REGIND(EAX))
	RET

/* get co-processor status word */
	GLOBL	GLNAME(ndp_getsw)
GLNAME(ndp_getsw):
	XOR_L	(EAX,EAX)
	FNSTSW	(AX)
	RET

/* clear co-processor error status */
	GLOBL	GLNAME(ndp_clearerr)
GLNAME(ndp_clearerr):
	FNCLEX
	RET

/* re-initialize co-processor to a known state */
	GLOBL	GLNAME(ndp_reinit)
GLNAME(ndp_reinit):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	SUB_L	(CONST(4),ESP)
	FNINIT
	FSTCW	(REGOFF(-4,EBP))
	AND_L	(CONST(ENOT(EXPR(NDP_INV|NDP_ZDIV|NDP_OVR|NDP_PC))),REGOFF(-4,EBP))
	OR_L	(CONST(EXPR(NDP_SIG53)),REGOFF(-4,EBP))
	FLDCW	(REGOFF(-4,EBP))
	MOV_L	(EBP,ESP)
	POP_L	(EBP)
	RET

/*
 * Miscellaneous support routines
 */

/*
 * Build a 80386 segment descriptor. The parameters are the base address
 * of the segment to be described, its size, its granularity, and its
 * access/type byte. These four parameters are all or-ed and shifted into
 * their correct place.
 */
init_descriptor:
	PUSH_L	(EAX)			/* base (0..31) */
	PUSH_L	(EBX)			/* GDT base pointer */
	PUSH_L	(ECX)			/* size (0..19) */
	PUSH_L	(EDX)			/* granularity/access/type */
	PUSH_L	(ESI)			/* descriptor pointer */
	MOV_W	(AX,REGOFF(2,ESI))	/* base (0..15) */
	SHR_L	(CONST(16),EAX)		/* unfortunately no bswap */
	MOV_B	(AL,REGOFF(4,ESI))	/* base (16..23) */
	MOV_B	(AH,REGOFF(7,ESI))	/* base (24..31) */
	AND_L	(CONST(0xFFFFF),ECX)	/* mask off segment limit */
	MOV_W	(CX,REGOFF(0,ESI))	/* limit (0..15) */
	SHR_L	(CONST(16),ECX)
	OR_B	(CL,REGOFF(6,ESI))	/* limit (16..19) */
	OR_B	(DL,REGOFF(6,ESI))	/* granularity */
	MOV_B	(DH,REGOFF(5,ESI))	/* access/type */
	POP_L	(ESI)
	POP_L	(EDX)
	POP_L	(ECX)
	POP_L	(EBX)
	POP_L	(EAX)
	RET

/* enable the use of pages */
	GLOBL	GLNAME(page_enable)
GLNAME(page_enable):
	MOV_L	(REGOFF(4,ESP),EAX)	/* page directory */
	MOV_L	(EAX,CR3)
	MOV_L	(CR0,EAX)		/* turn on paging */
	OR_L	(CONST(CR0_PG),EAX)
	MOV_L	(EAX,CR0)
	JMP	(page_enable1)		/* flush pre-fetch queue */
page_enable1:
	RET

/* disable the use of pages */
	GLOBL	GLNAME(page_disable)
GLNAME(page_disable):
	MOV_L	(CR0,EAX)		/* turn off paging */
	AND_L	(CONST(ENOT(CR0_PG)),EAX)
	MOV_L	(EAX,CR0)
 	XOR_L	(EAX,EAX)		/* clear page cache */
	MOV_L	(EAX,CR3)
	RET

/* wait for interrupt */
	GLOBL GLNAME(waitint)
GLNAME(waitint):
	STI
	HLT
	RET

/* enable interrupts */
	GLOBL GLNAME(enable)
GLNAME(enable):
	STI
	RET

/* disable interrupts */
	GLOBL GLNAME(disable)
GLNAME(disable):	
	CLI
	RET

/* get contents of flags register */
	GLOBL GLNAME(get_flags)
GLNAME(get_flags):
	PUSHF_L
	POP_L	(EAX)
	RET

/* set contents of flags register */
	GLOBL GLNAME(set_flags)
GLNAME(set_flags):
	MOV_L	(REGOFF(4,ESP),EAX)
	PUSH_L	(EAX)
	POPF_L
	RET

/* get contents of cr0 register */
	GLOBL GLNAME(getCR0)
GLNAME(getCR0):
	MOV_L	(CR0,EAX)
	RET

/* get contents of cr2 register */
	GLOBL GLNAME(getCR2)
GLNAME(getCR2):
	MOV_L	(CR2,EAX)
	RET

/* get contents of cr3 register */
	GLOBL GLNAME(getCR3)	
GLNAME(getCR3):
	MOV_L	(CR3,EAX)
	RET

/* set contents of cr0 register */
	GLOBL GLNAME(setCR0)
GLNAME(setCR0):
	MOV_L	(REGOFF(4,ESP),EAX)
	MOV_L	(EAX,CR0)
	RET

/* set contents of cr3 register */
	GLOBL GLNAME(setCR3)
GLNAME(setCR3):
	MOV_L	(REGOFF(4,ESP),EAX)
	MOV_L	(EAX,CR3)
	RET

/* flush TLB (by reloading cr3) */
	GLOBL GLNAME(flushTLB)
GLNAME(flushTLB):
	MOV_L	(CR3,EAX)
	MOV_L	(EAX,CR3)
	RET

/* output byte I/O */
	GLOBL GLNAME(out_byte)
GLNAME(out_byte):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	MOV_L   (REGOFF(8,EBP),EDX)
	MOV_L   (REGOFF(12,EBP),EAX)
	OUT_B
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* input byte I/O */
	GLOBL GLNAME(in_byte)
GLNAME(in_byte):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	MOV_L	(REGOFF(8,EBP),EDX)
	XOR_L	(EAX,EAX)
	IN_B
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* output word I/O */
	GLOBL GLNAME(out_word)
GLNAME(out_word):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	MOV_L   (REGOFF(8,EBP),EDX)
	MOV_L   (REGOFF(12,EBP),EAX)
	OUT_W
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* input word I/O */
	GLOBL GLNAME(in_word)
GLNAME(in_word):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	MOV_L	(REGOFF(8,EBP),EDX)
	XOR_L	(EAX,EAX)
	IN_W
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* output long word I/O */
	GLOBL GLNAME(out_long)
GLNAME(out_long):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	MOV_L   (REGOFF(8,EBP),EDX)
	MOV_L   (REGOFF(12,EBP),EAX)
	OUT_L
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* input long word I/O */
	GLOBL GLNAME(in_long)
GLNAME(in_long):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	MOV_L	(REGOFF(8,EBP),EDX)
	XOR_L	(EAX,EAX)
	IN_L
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* input byte string */
	GLOBL GLNAME(ins_byte)
GLNAME(ins_byte):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	PUSH_L	(EDI)
	PUSH_L	(ECX)
	MOV_L	(REGOFF(8,EBP),EDX)	/* port */
	MOV_L	(REGOFF(12,EBP),EDI)	/* pointer */
	MOV_L	(REGOFF(16,EBP),ECX)	/* count */
	CLD
	REP	INS_B			/* input byte */
	POP_L	(ECX)
	POP_L	(EDI)
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* output byte string */
	GLOBL GLNAME(outs_byte)
GLNAME(outs_byte):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	PUSH_L	(ESI)
	PUSH_L	(ECX)
	MOV_L	(REGOFF(8,EBP),EDX)	/* port */
	MOV_L	(REGOFF(12,EBP),ESI)	/* pointer */
	MOV_L	(REGOFF(16,EBP),ECX)	/* count */
	CLD
	REP	OUTS_B			/* output byte */
	POP_L	(ECX)
	POP_L	(ESI)
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* input word string */
	GLOBL GLNAME(ins_word)
GLNAME(ins_word):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	PUSH_L	(EDI)
	PUSH_L	(ECX)
	MOV_L	(REGOFF(8,EBP),EDX)	/* port */
	MOV_L	(REGOFF(12,EBP),EDI)	/* pointer */
	MOV_L	(REGOFF(16,EBP),ECX)	/* count */
	SAR_L	(CONST(1),ECX)		/* count in words */
	CLD
	REP	INS_W			/* input word */
	POP_L	(ECX)
	POP_L	(EDI)
	POP_L	(EDX)
	POP_L	(EBP)
	RET

/* output word string */
	GLOBL GLNAME(outs_word)
GLNAME(outs_word):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	PUSH_L	(EDX)
	PUSH_L	(ESI)
	PUSH_L	(ECX)
	MOV_L	(REGOFF(8,EBP),EDX)	/* port */
	MOV_L	(REGOFF(12,EBP),ESI)	/* pointer */
	MOV_L	(REGOFF(16,EBP),ECX)	/* count */
	SAR_L	(CONST(1),ECX)		/* count in words */
	CLD
	REP	OUTS_W			/* output word */
	POP_L	(ECX)
	POP_L	(ESI)
	POP_L	(EDX)
	POP_L	(EBP)
	RET

#ifdef KGDB
	GLOBL GLNAME(breakpoint)
GLNAME(breakpoint):
	PUSH_L	(EBP)
	MOV_L	(ESP,EBP)
	INT3
	POP_L	(EBP)
	RET
#endif /* KGDB */

/*
 * Crucial system data: be very careful when you change it
 */
	SEG_DATA
	GLOBL	GLNAME(gdt), GLNAME(kst_beg), GLNAME(kst_end), GLNAME(tss)
	ALIGNDATA4
GLNAME(gdt):
	D_SPACE	(EXPR(EMUL(GDT_SIZE,DESC_SIZE)))/* global descriptor table */
	ALIGNDATA4
GLNAME(kst_beg):
	D_SPACE	(K32_STACK_SIZE)		/* start kernel stack */
GLNAME(kst_end):
	D_SPACE	(4)				/* end kernel stack */
	ALIGNDATA4
GLNAME(tss):
	D_SPACE	(TSS_SIZE)			/* task state segment */
