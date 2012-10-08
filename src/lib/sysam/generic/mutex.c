/*	@(#)mutex.c	1.4	96/02/27 11:22:23 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/mutex.h"
#include "memaccess.h"

/*
 * In this mutex implementation, we try to avoid going to the kernel
 * unnecesarily, even with preemptive scheduling on.  We only assume
 * that an (architecture dependent) test-and-set instruction is available.
 * 
 * We divide the mutex in two parts:
 * - byte 0: to register that another thread was interested in the lock,
 *	     but could not get it
 * - byte 1: used to get the lock quickly using test-and-set
 */

#define MU_RESERVE_BYTE(mu)      ((volatile char *) mu)
#define MU_FASTLOCK(mu)         (((volatile char *) mu) + 1)

extern void _sys_mu_unlock _ARGS(( mutex * ));
extern int _sys_mu_trylock _ARGS(( mutex *, interval ));

int
mu_trylock(mu, t)
mutex   *mu;
interval t;
{
    /* First, just try to get the lock. */
    if (__ldstub(MU_FASTLOCK(mu)) == 0) {
	/* Simple case: we got the lock. */
	return 0;
    }

    /* Register that another thread is waiting for the lock. */
    *MU_RESERVE_BYTE(mu) = 1;	/* This should happen in the kernel.
				 * When it does, the next three lines
				 * are not needed.
				 */
    if (__ldstub(MU_FASTLOCK(mu)) == 0) {
	return 0;
    }

    return _sys_mu_trylock(mu, t);
}

void
mu_unlock(mu)
mutex *mu;
{
    *MU_FASTLOCK(mu) = 0;	/* Make mutex available. */

    /* Now, see if someone got interested in this lock.
     * If so, try to regrab the lock and unlock through the kernel.
     * If we cannot regrab the lock, another thread has taken it, no problem,
     * although it is possibly unfair.
     */
    if (*MU_RESERVE_BYTE(mu) != 0 && __ldstub(MU_FASTLOCK(mu)) == 0) {
    	_sys_mu_unlock(mu);
    }
}
