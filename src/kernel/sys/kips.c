/*	@(#)kips.c	1.4	96/02/27 14:21:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * calculate_kips
 *
 *	We calculate an approximation to the kips (kilo instructions per
 *	second) rating of the hardware we are running on.  The run server
 *	will need this info from us to schedule user processes across the
 *	available pool processors.
 *	We run for 500 ms through a loop and see how many times we get
 *	through it.  This is partly compiler dependent but we try to do
 *	things that are not easy for compilers to super-optimise.
 *
 * NB.  The clock interrupt must be running for this to work.  Otherwise
 *	it is just an infinite loop.  It's best to run this before too
 *	many other interrupts are running, though.
 *
 * Author:  Gregory J. Sharp, March 1993.
 */

#include "amoeba.h"
#include "debug.h"
#include "sys/proto.h"
#include "assert.h"
INIT_ASSERT

#ifndef __STDC__
#define	volatile static
#endif

extern unsigned long	milli_uptime;

uint16		kips;
static int	use_j;

static unsigned long
get_uptime()
{
    return milli_uptime;
}

static int
indirection(i)
unsigned long i;
{

    i >>= 2;
    i += use_j;
    return (int) i + 72;
}

void
calculate_kips()
{
    volatile int	j;
    unsigned long	i;	/* count of times through the loop */
    unsigned long	stop_time;
    int			(*funky)();
    int			watchdog;

    watchdog = 0;
    j = 0;
    i = 0;
    funky = indirection;
    checkints(); /* Get the clock up to date */
    stop_time = get_uptime() + 500;  /* We run for about 0.5 sec */
    do
    {
	/*  This loop is as long as it is to try to fox small caches */
	i++;
	j ^= i - 1972;
	j &= ~(i - j);
	j--;
	j = (~j + (*funky)(i+j)) >> 2;
	use_j = j;
	j *= 17;
	j -= 78;
	j |= i;
	j += (*funky)((unsigned long) 9);
	j >> 2;
	if ((i & 0xFF) == 0)
	{
	    checkints(); /* Let the clock run if it isn't hardware */
	    if (get_uptime() == (stop_time - 500))
	    {
		if (++watchdog >= 1000)
		    panic("calculate_kips: clock doesn't seem to be running\n");
	    }
	    else
		watchdog = 0;
	}
    } while (get_uptime() < stop_time);

    /* If j is not used here compilers will optimise away its calculation */
    use_j = j;

    /* To get kips divide # iterations by 8 */
    i >>= 3;
    if (i > 0xffff)
    {
	kips = 0xffff;
	printf("Kips rating of this machine exceeds %d\n", kips);
    }
    else
	kips = i & 0xffff;

    /* Round kips off to nearest hundred  - a 50 rounds up. */
    kips = (uint16) ((((int) kips + 50) / 100) * 100);
#ifdef LOWER_KIPS
    /* LOWER_KIPS may be defined in experimental optimised kernels,
     * to fake a lower kips rating.  This way we can reduce the chance
     * of it being selected by most user programs, without having to
     * remove it from many pools temporarily.
     */
    kips /= 2;
#endif

    DPRINTF(0, ("calculate_kips: j = 0x%x, i = 0x%x, kips = %d\n", j, i, kips));
    assert(kips != 0);
}
