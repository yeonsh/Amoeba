/*	@(#)pic.c	1.3	94/04/06 09:22:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * pic.c
 *
 * Driver for the two 8259 programmable interrupt controllers.
 *
 * This is a pretty simple driver and uses the default action of the 8259
 * to achieve interrupt priority. The hardware interrupts are prioritized 
 * according to which interrupt request line (irq) they are attached to. 
 *
 * Author:
 *	Leendert van Doorn
 */
#include <assert.h>
INIT_ASSERT
#include <amoeba.h>
#include <trap.h>
#include "i386_proto.h"
#include "pic.h"

#ifndef	PIC_DEBUG
#define	PIC_DEBUG	0
#endif

#ifndef NDEBUG
static int pic_debug;		/* current debug level */
#endif

static int pic_int1_imr;	/* interrupt mask for first PIC */
static int pic_int2_imr;	/* interrupt mask for second PIC */

/*
 * Initialize the programmable interrupt controller and mask out
 * all interrupts. The driver should enable an interrupt, when it
 * needs one.
 */
void
pic_init()
{
    register int flags;

#ifndef NDEBUG
    if ((pic_debug = kernel_option("pic")) == 0)
	pic_debug = PIC_DEBUG;
#endif

    /* master PIC: irq 0 - 7 */
    flags = get_flags(); disable();
    out_byte(PIC_INT1_CMD, PIC_ICW1);
    out_byte(PIC_INT1_IMR, PIC_ICW2_MASTER);
    out_byte(PIC_INT1_IMR, PIC_ICW3_MASTER);
    out_byte(PIC_INT1_IMR, PIC_ICW4_MASTER);
    out_byte(PIC_INT1_IMR, ~0 & ~PIC_ICW3_MASTER); /* OCW1 */
    out_byte(PIC_INT1_CMD, PIC_OCW3);

    /* slave PIC: irq 8 - 15 */
    out_byte(PIC_INT2_CMD, PIC_ICW1);
    out_byte(PIC_INT2_IMR, PIC_ICW2_SLAVE);
    out_byte(PIC_INT2_IMR, PIC_ICW3_SLAVE);
    out_byte(PIC_INT2_IMR, PIC_ICW4_SLAVE);
    out_byte(PIC_INT2_IMR, ~0); /* OCW1 */
    out_byte(PIC_INT2_CMD, PIC_OCW3);
    set_flags(flags);

#ifndef NDEBUG
    if (pic_debug > 0)
	printf("pic_init(), master %x, slave %x\n",
	    PIC_ICW2_MASTER, PIC_ICW2_SLAVE);
#endif
}

/*
 * Enable interrupts for specified irq level
 */
void
pic_enable(irq)
    int irq;
{
    register int flags;

    assert(irq >= 0 && irq < NIRQ);
    flags = get_flags(); disable();
    if (irq < 8)
	out_byte(PIC_INT1_IMR, in_byte(PIC_INT1_IMR) & ~(1 << irq));
    else
	out_byte(PIC_INT2_IMR, in_byte(PIC_INT2_IMR) & ~(1 << (irq-8)));
#ifndef NDEBUG
    if (pic_debug > 1)
	printf("pic_enable(%d) = %x\n",
	    irq, in_byte(irq < 8 ? PIC_INT1_IMR : PIC_INT2_IMR) & 0xFF);
#endif
    set_flags(flags);
}

/*
 * Disable interrupts for specified irq level
 */
void
pic_disable(irq)
    int irq;
{
    register int flags;

    assert(irq >= 0 && irq < NIRQ);
    flags = get_flags(); disable();
    if (irq < 8)
	out_byte(PIC_INT1_IMR, in_byte(PIC_INT1_IMR) | (1 << irq));
    else
	out_byte(PIC_INT2_IMR, in_byte(PIC_INT2_IMR) | (1 << (irq-8)));
#ifndef NDEBUG
    if (pic_debug > 1)
	printf("pic_disable(%d) = %x\n",
	    irq, in_byte(irq < 8 ? PIC_INT1_IMR : PIC_INT2_IMR) & 0xFF);
#endif
    set_flags(flags);
}

/*
 * Start all device interrupts again by restoring the
 * masks saved by pic_stop.
 */
void
pic_start()
{
    register int flags;

    flags = get_flags(); disable();
#ifndef NDEBUG
    if (pic_debug > 1)
	printf("pic_start(), imr1 %x, imr2 %x\n", pic_int1_imr, pic_int2_imr);
#endif
    out_byte(PIC_INT1_IMR, pic_int1_imr);
    out_byte(PIC_INT2_IMR, pic_int2_imr);
    set_flags(flags);
}

/*
 * Save the current interrupt masks so that they can be
 * restored later on, and stop all device interrupts.
 */
void
pic_stop()
{
    register int flags;

    flags = get_flags(); disable();
    pic_int1_imr = in_byte(PIC_INT1_IMR);
    pic_int2_imr = in_byte(PIC_INT2_IMR);
#ifndef NDEBUG
    if (pic_debug > 1)
	printf("pic_stop(), imr1 %x, imr2 %x\n", pic_int1_imr, pic_int2_imr);
#endif
    out_byte(PIC_INT1_IMR, ~0 & ~PIC_ICW3_MASTER);
    out_byte(PIC_INT2_IMR, ~0);
    set_flags(flags);
}
