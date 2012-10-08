/*	@(#)ndp.c	1.4	94/04/06 09:21:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * ndp.c
 *
 * Handles ISA/MCA driving peculiarities for the 387/486 ndp co-processor.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <i80386.h>
#include <fault.h>
#include "i386_proto.h"
#include <ndp.h>

#define	NDP_IRQ		13	/* co-processor interrupt */

/*
 * Initialize the ndp chip.
 * Because a lot of 486 have a broken external ndp interrupt,
 * we use exception 16 on 486 type machines.
 */
void
ndp_init()
{
    extern int cpu_type, ndp_type;
    extern void ndp_intr();

    if (ndp_type != NDP_NONE) {
	setCR0((getCR0() & ~CR0_EM) | CR0_MP | CR0_TS);
	if (cpu_type == CPU_386) {
	    /* use external chip wiring */
	    setirq(NDP_IRQ, ndp_intr);
	    pic_enable(NDP_IRQ);
	    out_byte(0xF1, 0); /* chip reset */
	}
	if (cpu_type == CPU_486) {
	    /* use internal chip wiring */
	    ndp_type = NDP_487;
	    setCR0(getCR0() | CR0_NE);
	}
    } else
	setCR0(getCR0() | CR0_EM);
}

/*
 * Handle co-processor error interrupt on an ISA/MCA machine.
 * Something to do with resetting the buffer chip between the
 * co-processor and CPU.
 */
void
ndp_intr(reason, frame)
    int reason;
    struct fault *frame;
{
    out_byte(0xF0, 0); /* clear buffer latch */
    (void) ndp_error(reason, frame);
}
