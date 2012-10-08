/*	@(#)picintr.S	1.2	94/04/06 09:23:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * picintr.S
 *
 * Pic_intr is called when entering a hardware interrupt handler. It
 * acknowledges the interrupt and does some bug workarounds on the fly.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Gregory J. Sharp, Oct 1992 - converted to new assyntax.h
 */
#include <assyntax.h>
#include <trap.h>
#include "pic.h"

#define	BUG_WORK_AROUND

#if (PIC_INT1_CMD > 0xFF) || (PIC_INT2_CMD > 0xFF)
error I/O ports are out of instruction range
#endif

	AS_BEGIN
	SEG_TEXT

/*
 * Enter device interrupt handler. 
 */
	GLOBL	pic_intr
pic_intr:
#ifdef BUG_WORK_AROUND
/* check for spurious interrupts that occur on IRQ 7 and IRQ 15 */
	MOV_L	(EBX,ECX)
	AND_L	(CONST(7),ECX)
	CMP_L	(CONST(7),ECX)
	JNE	(pic_intr3)

/* read interrupt status register */
	CMP_L	(CONST(8),EBX)
	JAE	(pic_intr0)
	IN1_B	(CONST(PIC_INT1_CMD))
	JMP	(pic_intr1)
pic_intr0:
	IN1_B	(CONST(PIC_INT2_CMD))
pic_intr1:
	TEST_B	(CONST(0x80), AL)
	JNZ	(pic_intr3)		/* it really happened */
	CMP_L	(CONST(8),EBX)
	JB	(pic_intr2)
	MOV_B	(CONST(PIC_NS_EOI),AL)	/* re-enable master */
	OUT1_B	(CONST(PIC_INT1_CMD))
pic_intr2:
	STC				/* failure */
	RET
#endif /* BUG_WORK_AROUND */

/* send non-specific EOI to master and slave (if necessary) */
pic_intr3:
	MOV_B	(CONST(PIC_NS_EOI),AL)
	OUT1_B	(CONST(PIC_INT1_CMD))
	CMP_L	(CONST(8),EBX)
	JB	(pic_intr4)
	OUT1_B	(CONST(PIC_INT2_CMD))
pic_intr4:
	CLC				/* success */
	RET
