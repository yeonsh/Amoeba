/*	@(#)pit.c	1.4	94/04/06 09:23:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * pit.c
 *
 * Driver for the 8254 timer (PIT). The i8254 timer has three timer channels
 * of which only one (channel 0) is available for timer interrupts. The other
 * channels are used for the speaker and memory refresh.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <assert.h>
INIT_ASSERT
#include <fault.h>
#include <bool.h>
#include "sys/proto.h"
#include "i386_proto.h"
#include "pit.h"

#ifndef PIT_DEBUG
#define	PIT_DEBUG	0
#endif

static int pit_debug;			/* current debug level */

#ifdef notyet
static uint32 pit_hw_milli();
#endif
static void pit_intr();

/*
 * Initialize channel 0 of the i8254A timer
 */
void
pit_init()
{
    register uint32 counter = (PIT_FREQ * PIT_INTERVAL) / 1000L;

#ifndef NDEBUG
    if ((pit_debug = kernel_option("pit")) == 0)
	pit_debug = PIT_DEBUG;
    if (pit_debug > 1)
	printf("pit_init(), pit_interval = 0x%x (%d), counter = %x\n",
	    PIT_INTERVAL, PIT_INTERVAL, counter);
#endif

    /* set timer to run continuously */
    assert(counter <= 0xFFFF);
    out_byte(PIT_MODE, PIT_MODE3);
    out_byte(PIT_CH0, (int) (counter & 0xFF));
    out_byte(PIT_CH0, (int) ((counter >> 8) & 0xFF));

#ifdef notyet
    /* I still have to thoroughly test this */
    set_hw_milli(pit_hw_milli);
#endif

    setirq(PIT_IRQ, pit_intr);
    pic_enable(PIT_IRQ);
}

/*
 * Read i8254's channel 0 counter. The counter decrements at twice the
 * timer frequency (one full cycle for each half of a square wave).
 */
uint16
pit_channel0()
{
    register uint16 counter;

    out_byte(PIT_MODE, PIT_LC);
    counter = in_byte(PIT_CH0), counter |= (in_byte(PIT_CH0) << 8);
    return counter;
}

/*
 * Delay for at least ``msec'' milli seconds
 */
void
pit_delay(msec)
    int msec;
{
    register uint16 current, previous, diff;
    register uint32 total;

    /*
     * The counter decrements at twice the timer frequency
     * (one full cycle for each half of a square wave).
     */
    diff = 100; /* just in case */
    total = (uint32) msec * (2 * PIT_FREQ / 1000);
    previous = pit_channel0();
    for (;;) {
	current = pit_channel0();
	if (current < previous)
	    diff = previous - current;
	if (diff >= total)
	    break;
	total -= diff;
	previous = current;
    }
}

#ifdef notyet
/*
 * Return number of actual milli-seconds that have passed
 */
static uint32
pit_hw_milli()
{
    extern uint32 milli_uptime;
    register uint32 milli;
    register int flags;
    uint16 counter;
    int status;

    flags = get_flags(); disable();
    out_byte(PIT_MODE, PIT_RB);
    out_byte(PIT_MODE, 0xC2);
    status = in_byte(PIT_CH0);
    counter = in_byte(PIT_CH0), counter |= (in_byte(PIT_CH0) << 8);
    milli = milli_uptime + (PIT_INTERVAL / 2) *
	(counter / (PIT_FREQ * PIT_INTERVAL) / 1000L);
    if ((status & 0x80) == 0) milli += PIT_INTERVAL/2;
    set_flags(flags);
    return milli;
}
#endif

/*
 * The actual clock interrupt
 */
/* ARGSUSED */
static void
pit_intr(reason, frame)
    int reason;
    struct fault *frame;
{
    void sweeper_run();
    void flp_motoroff();
    extern int motortime;

#ifdef MCA
    /* ps/2 clock needs to be told to stop interrupting */
    out_byte(0x61, in_byte(0x61) | 0x80);
#endif

    enqueue(sweeper_run, (long) PIT_INTERVAL);

#if (defined(ISA) || defined(MCA)) && !defined(NOFLOPPY)
    /* stop running floppy motor */
    if (motortime != 0 && --motortime == 0)
	flp_motoroff();
#endif
}
