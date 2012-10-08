/*	@(#)timer.c	1.4	96/02/27 13:54:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: timer.c -- set up and maintain a timer that will enqueue timer
 * requests at the rate of HZ. The sun4c has two counter-timers, each with
 * a counter register and a limit register. Setting the limit register
 * starts the timer, which interrupts when it has reached it's limit
 * register. Reading the limit register clears the interrupt and limit
 * reached bit (TC_LIMIT). Timer 0 uses interrupt 10, and timer 1 uses
 * interrupt 14. We use timer 0 to call timer_interrupt().
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#include "amoeba.h"
#include "global.h"
#include "mmuconst.h"
#include "machdep.h"
#include "fault.h"
#include "interrupt.h"
#include "map.h"
#include "sys/proto.h"
#include "debug.h"
#include "mmuproto.h"


#define	MICROSECS		1000000		/* micro seconds per second */
#define TIMER_SHIFT		10	       	/* Shift to get to hardware */
#define HZTOTIMER( hz )		((MICROSECS/hz) << TIMER_SHIFT )

struct timer_counters {
    unsigned	tc_count0;
    unsigned	tc_limit0;
    unsigned	tc_count1;
    unsigned	tc_limit1;
} *timers;

int timer_scrap;			/* Used to defeat compiler -O */
extern unsigned char *interrupt_reg_p;	/* interrupt enable register */


#ifdef PROFILE

/* We use the second counter timer to do profiling. */
void
prof_start_clock(milli)
interval	milli;	/* We want a clock tick every "milli" milliseconds */
{
    /* Convert miliseconds to microseconds and then put into format for
     * limit register
     */
    milli = (milli * 1000) << TIMER_SHIFT;
    timers->tc_limit1 = milli;
    timer_scrap = timers->tc_limit1;
    *interrupt_reg_p |= IER_HARD14;
}

void
prof_stop_clock()
{
    /* Disable timer1 interrupt */
    *interrupt_reg_p &= ~IER_HARD14;
}

/*ARGSUSED*/
void
profile_interrupt( tt, sp, pc, psr, pid )
int tt;					/* Should be INTERUPT( timer ) */
thread_ustate *sp;			/* thread be interrupted */
int pc;					/* ... and it's pc */
int psr;				/* ... and it's psr */
int pid;				/* Process ID number */
{
    timer_scrap = timers->tc_limit1;
    profile_timer( sp, pc, psr );
}

#endif /* PROFILE */

/*
 * timer_interrupt (INTERRUPT) -- enqueue a timer request for handling by
 * upper level code at some later time.  We also read the limit register,
 * to reset the counter.
 */
/*ARGSUSED*/
void
timer_interrupt( tt, sp, pc, psr, pid )
int tt;					/* Should be INTERUPT( timer ) */
thread_ustate *sp;			/* thread be interrupted */
int pc;					/* ... and it's pc */
int psr;				/* ... and it's psr */
int pid;				/* Process ID number */
{
    timer_scrap = timers->tc_limit0;

    enqueue( sweeper_run, 1000L/TICKS_PER_SECOND );
}

/*
 * inittimer() (ENTRY POINT) -- initialize anything that the timer might need.
 */
void
inittimer() {

    timers = (struct timer_counters *) mmu_virtaddr("/counter-timer");

    setvec( INT_CNTR0, timer_interrupt );

    /* initialize the timer ... */
    timers->tc_limit0 = HZTOTIMER( TICKS_PER_SECOND );
    timer_scrap = timers->tc_limit0;

#ifdef PROFILE
    setvec( INT_CNTR1, profile_interrupt );
#endif

    /* ... and enable the interrupt controller */
    *interrupt_reg_p |= IER_HARD10;
}
