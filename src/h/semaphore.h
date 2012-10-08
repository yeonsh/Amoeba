/*	@(#)semaphore.h	1.4	95/05/17 09:43:12 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

/* Counting semaphores */

/* This is an implementation of counting semaphores using only mutexes.
   Moreover, *IF* mutexes are implemented in a way that requires no
   system calls when there is no contentition, the same will be true for
   the counting semaphores.
   
   NOTE: calling sema_init is compulsory before using a semaphore!
   
   Author: Guido van Rossum, CWI, November/December 1988
   Modified: Jack Jansen, March 1989 -  added sema_mdown and sema_mup
   Modified: Gregory J. Sharp, May 1995 - kernel has non-preemptive
		    scheduling so sema_mu is not needed there.  Furthermore,
		    if it is there it means sema_*up can't be used in enqueued
		    interrupt handlers since they cannot call mu_lock. Hence
		    the #ifndef KERNEL
*/

typedef struct {
	/* Don't look inside semaphores; the structure's contents
	   is only published so you can declare semaphore variables */
	mutex sema_lock;
#ifndef KERNEL
	mutex sema_mu;
#endif
	unsigned int sema_lev;
} semaphore;

/* Invariants:
	- sema_level may only be changed or inspected while mu is held
	- sema_level >= 0
	- sema_lock is held iff sema_level is 0 (outside critical sections)
*/

#define	sema_init	_sema_init
#define	sema_down	_sema_down
#define	sema_mdown	_sema_mdown
#define	sema_trydown	_sema_trydown
#define	sema_trymdown	_sema_trymdown
#define	sema_up		_sema_up
#define	sema_mup	_sema_mup
#define	sema_level	_sema_level

void sema_init _ARGS(( semaphore *sema, int level ));
/* Initialize a semaphore to a given level (>= 0); *must* be used! */

void sema_down _ARGS(( semaphore *sema ));
/* Decrease the semaphore's level, block while level == 0 */

int sema_mdown _ARGS(( semaphore *sema, int count ));
/* Multiple down, blocking at most once.  Returns actual down count. */

int sema_trydown _ARGS(( semaphore *sema, long maxdelay ));
/* Like sema_down but with max. delay (in milliseconds).
   Returns 0 if successful, <0 if timed out. */

int sema_trymdown _ARGS(( semaphore *sema, int count, interval maxdelay ));
/* Combines sema_trydown and sema_mdown.
   Returns actual count down if successful, <0 if timed out. */

void sema_up _ARGS(( semaphore *sema ));
/* Increase the semaphore's level, wake up blocked sema_down calls */

void sema_mup _ARGS(( semaphore *sema, int count ));
/* Multiple up */

int sema_level _ARGS(( semaphore *sema ));
/* Return the semaphore's level */

#endif /* __SEMAPHORE_H__ */
