/*	@(#)khead.S	1.7	94/04/06 09:39:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * khead.S
 *
 * The Amoeba 80386 kernel runs in 32-bit mode, but it may be started in
 * 16-bit mode. The run-time start off code in this file warps the CPU
 * from 16-bit mode into 32-bit mode (by building a temporary GDT) and
 * jump to kstart which is the actual 32-bit kernel start off. The code
 * is somewhat blurred by the Amoeba reboot code: in that case the kernel
 * already runs in 32-bit mode and no mode warp is needed. To determine
 * whether the kernel runs in 16 or 32 bit mode the protection bit in the
 * CR0 register is examined. Since the Amoeba kernel only has this bit on
 * when running in 32-bit mode it is safe to use it.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Gregory J. Sharp Oct 1992 - converted to new assyntax.h form.
 *	Leendert van Doorn Oct 1992 - I removed the nasty fire-breathing
 *		dragons (according to Greg:-) to make live easier for
 *		all of us.
 */
#include <assyntax.h>
#include <asdebug.h>
#include <i80386.h>

#define	K16_STACK_SIZE	128	/* size of 16-bit kernel stack */

/*
 * Many 386 assemblers can't generate 16-bit mode instructions which would
 * be nice to use in certain places, so we use operand size and address
 * prefixes to make the machine execute only 32-bit instructions even when
 * it is running in 16-bit mode.
 */
#define	ADDR32	D_BYTE 0x67
#define	DATA32	D_BYTE 0x66

	AS_BEGIN

	SEG_TEXT

	/**************************************************/
	/* Kernel can be started in 16-bit or 32-bit mode */
	/**************************************************/

/*
 * This is the main 16/32-bit entry point of the kernel.  Only instructions
 * which are identical in both modes can be used. i.e. - only registers
 * may be accessed.
 */
	GLOBL	GLNAME(kernelstart)
GLNAME(kernelstart):
	CLI				/* disable interrupts */
	CLD				/* autoincrement up */

/* determine whether we are in 16 or 32 bit mode */
	MOV_L	(CR0, EAX)		/* examine protected mode bit */
	RCR_L	(CONST(1), EAX)
	JNC	(kernelstart1)		/* assembler must output 8-bit jump! */

	/*********************************/
	/* This code runs in 32-bit mode */
	/*********************************/

/*
 * We were already running in 32-bit mode.
 * Build boot global descriptor table.
 */
	MOV_L	(CONST(BOOT_GDTBASE),EBX)
	MOV_L	(CONST(0), REGOFF(BOOT_NULL_SELECTOR+0, EBX))
	MOV_L	(CONST(0), REGOFF(BOOT_NULL_SELECTOR+4, EBX))
	MOV_L	(CONST(0x0000FFFF), REGOFF(BOOT_DS_SELECTOR+0, EBX))
	MOV_L	(CONST(0x00CF9200), REGOFF(BOOT_DS_SELECTOR+4, EBX))
	MOV_L	(CONST(0x0000FFFF), REGOFF(BOOT_CS_SELECTOR+0, EBX))
	MOV_L	(CONST(0x00CF9A00), REGOFF(BOOT_CS_SELECTOR+4, EBX))

/* force CPU shutdown on any (unexpected) interrupt */
	MOV_L	(CONST(BOOT_GDTPTR),EBX)
	MOV_W	(CONST(0), REGOFF(0,EBX))
	MOV_L	(CONST(0), REGOFF(2,EBX))
	LIDT	(REGIND(EBX))

/* initialize global descriptor table */
	MOV_W	(CONST(EMUL(BOOT_GDT_SIZE,DESC_SIZE)-1), REGOFF(0,EBX))
	MOV_L	(CONST(BOOT_GDTBASE), REGOFF(2,EBX))
	LGDT	(REGIND(EBX))

/* initialize segment registers */
	MOV_L	(CONST(BOOT_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	MOV_SR	(AX,SS)
	MOV_SR	(AX,FS)
	MOV_SR	(AX,GS)

/* move new kernel to low memory */
	MOV_L	(REGOFF(4,ESP),ESI)
	MOV_L	(REGOFF(8,ESP),EDI)
	MOV_L	(REGOFF(12,ESP),ECX)
	REP	MOVS_B

/* jump to 32-bit kernel start off */
	JMPF	(CONST(BOOT_CS_SELECTOR),ADDR(GLNAME(kstart)))


	/*********************************/
	/* This code runs in 16-bit mode */
	/*********************************/

/*
 * Running in 16-bit mode. Setup data, extra, stack segment, and stack
 * pointer registers for the time we are running in this mode.
 * Remember, we got here through a far jump 0:0x2000, and the interrupts
 * are still disabled.
 */
kernelstart1:
	MOV_SR	(CS,AX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	MOV_SR	(AX,SS)
	DATA32
	MOV_L	(ADDR(kst_16bit),ESP)		/* set stack pointer */

#if defined(ISA) || defined(MCA)
/*
 * Use the BIOS to kick us into protected mode. This is the most
 * portable way to enable the A20 address line feature.
 */
	DATA32
	MOV_L	(ADDR(switchgdt),ESI)
	XOR_L	(BX,BX)
	MOV_B	(CONST(0x89),AH)
	INT	(CONST(0x15))
#else
/* load the GDTR and IDTR */
	DATA32
	MOV_L 	(ADDR(switchgdt),EBX)
	ADDR32
	DATA32
	LGDT	(REGOFF(TEMP_GDT_SELECTOR,EBX))
	ADDR32
	DATA32
	LIDT	(REGOFF(TEMP_IDT_SELECTOR,EBX))

/* now jump to protected mode */
	MOV_L	(CR0,EAX)
	DATA32
	OR_L	(CONST(CR0_PROT),EAX)
	MOV_L	(EAX,CR0)

/* flush the instruction queue */
	JMP	(kernelstart2)
kernelstart2:	

/* jump into 32-bit mode */
	DATA32
	JMPF	(CONST(TEMP_CS_SELECTOR),ADDR(kernelstart3))
kernelstart3:
#endif /* defined(ISA) || defined(MCA) */


	/*******************************/
	/* This code is in 32-bit mode */
	/*******************************/

/* (re)load all segment registers */
	MOV_L	(CONST(TEMP_DS_SELECTOR),EAX)
	MOV_SR	(AX,DS)
	MOV_SR	(AX,ES)
	MOV_SR	(AX,SS)
	MOV_SR	(AX,FS)
	MOV_SR	(AX,GS)

/* jump to 32-bit kernel start off */
 	JMP	(GLNAME(kstart))


/*
 * A small stack is needed for the time we run in 16-bit mode. The
 * main reason for having this special stack is that it must lie
 * within 64Kb from kernelstart.
 */
	SEG_TEXT				/* !! */
	D_SPACE(K16_STACK_SIZE)
kst_16bit:

/*
 * Temporary GDT for switching to 32-bit mode. The layout conforms to
 * what BIOS expects. Note that this temporary GDT should work for other
 * bus architectures as well. Note that this data structure must lie
 * within 64Kb from kernelstart.
 */
	SEG_TEXT				/* !! */
	ALIGNDATA4
switchgdt:
/* NULL segment descriptor */
	D_LONG	0, 0

/* GDT segment descriptor */
	D_WORD	EMUL(TEMP_GDT_SIZE,DESC_SIZE)-1	/* descriptor size */
	D_LONG	switchgdt			/* address */
	D_WORD	0

/* IDT segment descriptor */
	D_LONG	0, 0

/* DS segment descriptor */
	D_WORD	SEG_SIZE&0xFFFF			/* segment limit */
	D_WORD	0				/* base 0..15 */
	D_BYTE	0				/* base 16..23 */
	D_BYTE	KDATA_ACC			/* access/type */
	D_BYTE	DATA_GRAN|EXPR(SEG_SIZE>>16)	/* granularity */
	D_BYTE	0				/* base 24..31 */
	
/* ES segment descriptor */
	D_WORD	SEG_SIZE&0xFFFF			/* segment limit */
	D_WORD	0				/* base 0..15 */
	D_BYTE	0				/* base 16..23 */
	D_BYTE	KDATA_ACC			/* access/type */
	D_BYTE	DATA_GRAN|EXPR(SEG_SIZE>>16)	/* granularity */
	D_BYTE	0				/* base 24..31 */

/* SS segment descriptor */
	D_WORD	SEG_SIZE&0xFFFF			/* segment limit */
	D_WORD	0				/* base 0..15 */
	D_BYTE	0				/* base 16..23 */
	D_BYTE	KDATA_ACC			/* access/type */
	D_BYTE	DATA_GRAN|EXPR(SEG_SIZE>>16)	/* granularity */
	D_BYTE	0				/* base 24..31 */
	
/* CS segment descriptor */
	D_WORD	SEG_SIZE&0xFFFF			/* segment limit */
	D_WORD	0				/* base 0..15 */
	D_BYTE	0				/* base 16..23 */
	D_BYTE	KCODE_ACC			/* access/type */
	D_BYTE	CODE_GRAN|EXPR(SEG_SIZE>>16)	/* granularity */
	D_BYTE	0				/* base 24..31 */

/* BIOS segment descriptor */
	D_LONG	0, 0

/*
 * This definition is crucial. We want begin to be the first symbol in bss
 * so that clearbss() can clear it. However some assemblers (especially
 * GNU) make it very hard to do this since they have no .bss directive.
 * I wonder whether the same trick works for system V assemblers.
 */
#ifdef GNU_ASSEMBLER
	GLOBL	GLNAME(begin)
COMM GLNAME(begin),	4
#else /* not GNU_ASSEMBLER */
	SEG_BSS					/* !! */
	GLOBL	GLNAME(begin)
GLNAME(begin):
	D_SPACE(4)

#ifdef ACK_ASSEMBLER
	/* Special ACK stuff */
	GLOBL hol0, .trppc, ECASE
	ECASE = 20
hol0:	D_SPACE(8)
.trppc: D_SPACE(4)
#endif /* ACK_ASSEMBLER */
#endif /* GNU_ASSEMBLER */
