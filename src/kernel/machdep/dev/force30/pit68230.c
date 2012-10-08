/*	@(#)pit68230.c	1.8	96/02/27 13:47:55 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <sys/proto.h>

#include <machdep.h>
#include <arch_proto.h>
#include <memaccess.h>

#include "global.h"
#include "pit68230.h"
#include "pit68230info.h"
#include "assert.h"
INIT_ASSERT

#define HZ		100	/* Could be made variable */

/*
 * The timer seems to be ~10% fast. I don't know how or why.
 * Temporarily patched up below, but...
 * Someone should look at this. Hans.
 */

#define REALCLOCKFREQ	3686400
#define CLOCKFREQ	(REALCLOCKFREQ*11/10)

#define DIVIDER		(CLOCKFREQ/16/(2*HZ))
#define DIVIDER_L	((DIVIDER)&0xFF)
#define DIVIDER_M	(((DIVIDER)>>8)&0xFF)
#define DIVIDER_H	(((DIVIDER)>>16)&0xFF)

#define PIT	ABSPTR(struct pit68230 *, pit68230info.pit_devaddr)
#define PIT2	ABSPTR(struct pit68230 *, pit2_68230info.pit_devaddr)

static void inttimer _ARGS(( int sp ));
void inittimer _ARGS(( void ));
void init_prof_timer _ARGS(( void ));
void set_hw_milli _ARGS(( unsigned long (*f)() ));

static volatile unsigned long upmilli;

unsigned long pit68230_getmilli()
{
    register volatile struct pit68230 *dev = PIT;
    register uint8 h, l, m;
    unsigned long time;
    unsigned long timebase;
    int cycle, precycle;

    /* Interrupts must be on! */
    assert(!(getsr() & 0x700));

    /* Read the counter while the timer is running.
     * The timer generates a block signal; the counter reaches zero twice
     * per one clock cycle.  This is actually causing a race since we need
     * to see if we are on the second countdown or the first and we can't
     * do this at exactly the same time we read out the clock.  Actually
     * we can't even read out the entire clock in a single cycle so we check
     * to make sure it hasn't wrapped before we accept what it offers.
     */
    do {
	timebase = mem_get_long(&upmilli);
	precycle = mem_get_byte(&dev->pit_pcdr) & 0x8;
	h = mem_get_byte(&dev->pit_cnrth);
	m = mem_get_byte(&dev->pit_cnrtm);
	l = mem_get_byte(&dev->pit_cnrtl);
	cycle = mem_get_byte(&dev->pit_pcdr) & 0x8;
    } while ((mem_get_byte(&dev->pit_cnrtm) != m) ||
	     (mem_get_byte(&dev->pit_cnrth) != h) ||
	     mem_get_long(&upmilli) != timebase || cycle != precycle);

    cycle = !cycle; /* if the bit was set there was no cycle yet! */

    time = (h << 16) + (m << 8) + l + cycle * DIVIDER;
    /* The counter counts down, so subtract the value from 1000L/HZ. */
    time = 1000L/HZ - 1000L/(2*HZ) * time / DIVIDER;
    time += timebase;
#ifdef DEBUG
    {
	static unsigned long last_time;

	compare(time, >=, last_time);
	last_time = time;
    }
#endif
    return time;
}

unsigned long getmicro()
{
    register volatile struct pit68230 *dev = PIT;
    register uint8 h, l, m;
    unsigned long time;
    unsigned long timebase;
    int cycle, precycle;

    /* Read the counter while the timer is running.
     * The timer generates a block signal; the counter reaches zero twice
     * per one clock cycle.  This is actually causing a race since we need
     * to see if we are on the second countdown or the first and we can't
     * do this at exactly the same time we read out the clock.  Actually
     * we can't even read out the entire clock in a single cycle so we check
     * to make sure it hasn't wrapped before we accept what it offers.
     */
    do {
	timebase = mem_get_long(&upmilli);
	precycle = mem_get_byte(&dev->pit_pcdr) & 0x8;
	h = mem_get_byte(&dev->pit_cnrth);
	m = mem_get_byte(&dev->pit_cnrtm);
	l = mem_get_byte(&dev->pit_cnrtl);
	cycle = mem_get_byte(&dev->pit_pcdr) & 0x8;
    } while ((mem_get_byte(&dev->pit_cnrtm) != m) ||
	     (mem_get_byte(&dev->pit_cnrth) != h) ||
	     mem_get_long(&upmilli) != timebase || cycle != precycle);

    cycle = !cycle; /* if the bit was set there was no cycle yet! */

    /* Shift the whole thing 8 bits left to get more precission. */
    time = (h << 16) + (m << 8) + l + cycle*DIVIDER;
    /* The counter counts down, so subtract the value from 1000000L/HZ. */
    time = 100000L/HZ - 100000L/(2*HZ) * time/DIVIDER;
    return(1000 * timebase + 10 * time);
}

/*ARGSUSED*/
static void
inttimer(sp) 
int sp;
{
    upmilli += 1000L/HZ;
    enqueue(sweeper_run, 1000L/HZ);
}


void
inittimer() {
	register volatile struct pit68230 *dev = PIT;

	mem_put_byte(&dev->pit_tcr, 0xA0);	/* disable timer */
	mem_put_byte(&dev->pit_pgcr, 0x30);	/*H4 ints enabled, port mode 0*/
	mem_put_byte(&dev->pit_psrr, 0x08);
	mem_put_byte(&dev->pit_pacr, 0xC0);
	mem_put_byte(&dev->pit_paddr, 0);	/* all inputs */

	mem_put_byte(&dev->pit_pbcr, 0xC0);	/* submode 1X */
	mem_put_byte(&dev->pit_pbddr, 0xFF);	/* all outputs */
	mem_put_byte(&dev->pit_cprl, DIVIDER_L);
	mem_put_byte(&dev->pit_cprm, DIVIDER_M);
	mem_put_byte(&dev->pit_cprh, DIVIDER_H);
	(void) setvec(pit68230info.pit_vector, inttimer);
	/* timer interrupt vector: */
	mem_put_byte(&dev->pit_tivr, pit68230info.pit_vector);
	mem_put_byte(&dev->pit_tcr, 0x41);	/* enable timer */
	set_hw_milli(pit68230_getmilli);
}


#if PROFILE
void
init_prof_timer()
{
	struct pit68230 *dev= PIT2;

	dev->pit_tcr = 0xA0;		/* disable timer */
	dev->pit_pgcr = 0x30;		/* H4 ints enabled, port mode 0 */
	dev->pit_psrr = 0x08;
	dev->pit_pacr = 0xC0;
	dev->pit_paddr = 0;		/* all inputs */

	dev->pit_pbcr = 0xC0;		/* submode 1X */
	dev->pit_pbddr = 0xFF;		/* all outputs */

#ifdef __STDC__
	(void) setvec(pit2_68230info.pit_vector, (void (*)(int)) profile_timer);
#else
	(void) setvec(pit2_68230info.pit_vector, profile_timer);
#endif
	dev->pit_tivr = pit2_68230info.pit_vector;/* timer interrupt vector */

	/* We don't start the timer yet, wait until a start_prof_tim is 
	 * issued 
	 */
}

void start_prof_tim(interv)
long interv;
{
	struct pit68230 *dev;
	long divider;

	dev= PIT2;
	divider= (CLOCKFREQ/16)/2*interv/1000;

	dev->pit_cprl = divider & 0xff;
	dev->pit_cprm = (divider >> 8) & 0xff;
	dev->pit_cprh = (divider >> 16) & 0xff;
	dev->pit_tcr = 0x41;		/* enable timer */
}

void stop_prof_tim()
{
	struct pit68230 *dev= PIT2;

	dev->pit_tcr = 0xA0;		/* disable timer */
}
#endif
