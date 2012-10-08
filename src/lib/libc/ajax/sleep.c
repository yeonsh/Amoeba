/*	@(#)sleep.c	1.6	96/02/27 11:06:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix sleep() function */

#include "ajax.h"
#include "module/syscall.h"


void
millisleep(nmilli)
	interval nmilli;
{
	if (nmilli > 0) {
		mutex mu;

		mu_init(&mu);
		mu_lock(&mu);
		(void) mu_trylock(&mu, nmilli);
	}
}

unsigned int
sleep(nsec)
	unsigned int nsec;
{
	interval t0, delay, t1;
	unsigned int time_used;
	
	/* All the complexity here is so that sleep will return the
	   amount of seconds left, as required by Posix */
	
	delay = nsec * TICKSPERSEC;
	t0 = sys_milli();
	millisleep(delay);
	t1 = sys_milli();
	time_used = (t1 - t0 + TICKSPERSEC/8) / TICKSPERSEC;
	return (time_used > nsec) ? 0 : nsec - time_used;
}

void
usleep(usec)
	unsigned int usec;
{
	/* Sleep for the indicated number of micro seconds.
	 * Under Amoeba we only have milli seconds resolution,
	 * but that's good enough for most purposes.
	 */
	millisleep(usec / 1000);
}

