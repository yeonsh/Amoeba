/*	@(#)trap.h	1.2	94/04/06 15:59:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TRAP_H__
#define __TRAP_H__

/*
 * Number of hardware interrupts
 */
#define	NIRQ		16

/*
 * Suitable irq bases for hardware interrupts.
 */
#define IRQ0_VECTOR     0x20	/* 0 - 0x1F are reserved */
#define IRQ8_VECTOR     0x28 	/* together for simplicity */

/*
 * i80386 CPU traps
 */
/* exception vector numbers: 0 -> 16 */
#define	TRAP_DIVIDE		0	/* division by zero */
#define	TRAP_DEBUG		1	/* debug exceptions */
#define	TRAP_NMI		2	/* non maskable interrupt */
#define	TRAP_BREAKPOINT		3	/* breakpoint */
#define	TRAP_OVERFLOW		4	/* overflow */
#define	TRAP_BOUNDS		5	/* bounds check */
#define	TRAP_INVALIDOP		6	/* invalid opcode */
#define	TRAP_NDPNA		7	/* NDP not available */
#define	TRAP_DOUBLEFAULT	8	/* double fault */
#define	TRAP_OVERRUN		9	/* NDP segment overrun (386 only)*/
#define	TRAP_INVALIDTSS		10	/* invalid TSS */
#define	TRAP_SEGNOTPRESENT	11	/* segment not present */
#define	TRAP_STACKFAULT		12	/* stack fault */
#define	TRAP_GENPROT		13	/* general protection */
#define	TRAP_PAGEFAULT		14	/* page fault */
#define	TRAP_NDPERROR		16	/* NDP error */
#define	TRAP_ALIGNMENT		17	/* alignment check (486 only) */
#define	TRAP_INVALIDTRAP	256	/* general invalid trap */

/* hardware vector numbers: 32 -> 48 */

/* system call numbers: 50 -> 63 */

#define NUM_VECTORS	64

#endif /* __TRAP_H__ */
