/*	@(#)interrupt.h	1.3	94/04/06 16:41:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: interrupt.h -- defines and structures for various interrupts
 */
#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "fault.h"

#define INT_IRQ1	INTERRUPT( 1 )
#define INT_SOFT1	INTERRUPT( 1 )
#define INT_IRQ2	INTERRUPT( 2 )
#define INT_SCSI	INTERRUPT( 3 )
#define INT_DVMA	INTERRUPT( 3 )
#define INT_IRQ3	INTERRUPT( 3 )
#define INT_SOFT4	INTERRUPT( 4 )
#define INT_LANCE	INTERRUPT( 5 )
#define INT_IRQ4	INTERRUPT( 5 )
#define INT_SOFT6	INTERRUPT( 6 )
#define INT_VIDEO	INTERRUPT( 7 )
#define INT_IRQ5	INTERRUPT( 7 )
/* Interrupt 8 is unused */
#define INT_IRQ6	INTERRUPT( 9 )
#define INT_CNTR0	INTERRUPT( 10 )
#define INT_FLOPPY	INTERRUPT( 11 )
#define INT_SERIAL	INTERRUPT( 12 )
#define INT_IRQ7	INTERRUPT( 13 )
#define INT_CNTR1	INTERRUPT( 14 )
#define INT_MEMORY	INTERRUPT( 15 )

#ifndef _ASM

typedef struct {
	unsigned int	ii_vector;
} intrinfo;

#endif	/* _ASM */
#endif	/* __INTERRUPT_H__ */
