/*	@(#)mutex.c	1.2	94/04/07 14:07:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**	People shouldn't need mutexes under Unix since there is no possibility
**	for multi-threaded processes, but just in case someone wants to run
**	something single threaded under Unix we include this for them.
**	(It is a programming error if a single threaded program tries to lock
**	a mutex twice, except when it is to simulate a timeout using trylock!)
*/

#include "amoeba.h"
#include "module/mutex.h"
#include "stdlib.h"

#define BUSY		1	/* within critical section */

void
mu_lock(crit)
mutex *	crit;
{
    if (*crit & BUSY)
	abort();	/* programming error */
    *crit |= BUSY;
}

int
mu_trylock(crit, timer)
mutex *		crit;
interval	timer;
{
    extern unsigned int sleep();
    
    if (*crit & BUSY)
    {
	if (timer > 0)
	    sleep((unsigned int) ((timer + 9) / 10));
	return -1;
    }
    *crit |= BUSY;
    return 0;
}

void
mu_unlock(crit)
mutex *	crit;
{
    *crit &= ~BUSY;
}

void
mu_init(crit)
mutex *	crit;
{
    *crit = 0;
}
