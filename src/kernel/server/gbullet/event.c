/*	@(#)event.c	1.1	96/02/27 14:07:10 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *	Author: Ed Keizer, 1995
 */

/* A module that creates something that looks like sleep/wakeup */
/* A consumer thread can announce that it will go to sleep on an event
   by calling event_incr. After event_incr returns the thread can perform
   a few actions. It then MUST call event_wait or event_trywait.
   These will return when the event has happened or the time timed out.
   The producer thread will notice that an event has happened.
   It can then call event_wake. It not not necessary for the producer
   to first lock the event structure, then perform the actual event
   and then wake up the waiting threads.
 */

/* A secondary and completely independent set of routines to sleep
 * on number/number was later added.
 */

#include "event.h"
/* The whole next bunch is here just to define dbmessage from bs_protos.h
   ....
 */
#include <stdlib.h>
#include "module/disk.h"
#include "amoeba.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "alloc.h"
#include "stats.h"
#include "cache.h"
#include "bs_protos.h"

#ifndef _ARGS
/* if _ARGS is defined, the guess is that bs_protos has been included */
extern void bwarn();
#endif

/* Defines and declarations for numbered events */
#define NUMEVENTS	40
static b_event		numb_events[NUMEVENTS] ;
static mutex		numb_mutex ;
static int		numb_inuse[NUMEVENTS] ;
static semaphore	numb_overflow ;
static int		numb_ov_cnt ;

/* Initialize the event structure */

void
event_init(p_event)
	b_event	*p_event ;
{
	sema_init(&p_event->guard,1) ;
	sema_init(&p_event->sleeper,0) ;
	p_event->count=0 ;
}

/* Announce the wait for an event */
/* Guard increments of count */
void
event_incr(p_event)
	b_event	*p_event ;
{
	sema_down(&p_event->guard) ;
	p_event->count++ ;
	sema_up(&p_event->guard) ;
}

/* Wait for an event to happen, must have been announced through
   event_incr.
 */
void
event_wait(p_event)
	b_event	*p_event ;
{
	sema_down(&p_event->sleeper) ;
}

/* Wait some time for an event to happen, must have been announced through
   event_incr.
 */
int
event_trywait(p_event,maxdelay)
	b_event	*p_event ;
	interval maxdelay ;
{
	if ( sema_trydown(&p_event->sleeper,maxdelay)<0 ) {
		/* Timed out, cancel event */
		event_cancel(p_event);
		return -1 ;
	}
	return 0 ;
}

/* Wake up all that are waiting or have announced that they will
   start to wait.
 */

void
event_wake(p_event)
	b_event	*p_event ;
{
	sema_down(&p_event->guard) ;
	sema_mup(&p_event->sleeper,p_event->count) ;
	p_event->count= 0 ;
	sema_up(&p_event->guard) ;
}

/* The calling thread was planning to wait, but gave up
 */

void
event_cancel(p_event)
	b_event	*p_event ;
{
	sema_down(&p_event->guard) ;
	if ( p_event->count>0 ) {
		p_event->count-- ;
	} else {
		if ( sema_trydown(&p_event->sleeper,(interval)0)<0 ) {
			/* Should not happen */
			bwarn("Could not cancel an event") ;
		}
	}
	sema_up(&p_event->guard) ;
}

/* Routines that allocate events and allow mapping form event number
 * to event address. Event numbers belong to group member numbers.
 */

/* Try to find a free event, return its number and address */
int	get_event(pp_event)
	b_event **pp_event ;
{
	int	i ;

	for (;;) {
		mu_lock( &numb_mutex ) ;
		for ( i=0 ; i<NUMEVENTS ; i++ ) {
			if ( numb_inuse[i]==0 ) {
				numb_inuse[i]=1 ;
				mu_unlock( &numb_mutex ) ;
				*pp_event= &numb_events[i] ;
				event_init(*pp_event) ;
				return i ;
			}
		}
		/* No event found, wait for wakeup */
		if ( !numb_ov_cnt ) {
			sema_init( &numb_overflow, 0 ) ;
			numb_ov_cnt++ ;
		}
		mu_unlock( &numb_mutex ) ;
		sema_down( &numb_overflow ) ;
	}
}

/* Find the event pointer belonging to an event number */
/* return 0 if not found */
b_event *
map_event(event_num)
	int		event_num ;
{
	/* We could do a lot of checking, but we don't */
	return &numb_events[event_num] ;
}

errstat
free_event(event_num)
	int		event_num ;
{

	mu_lock( &numb_mutex ) ;
	numb_inuse[event_num]=0 ;
	if ( numb_ov_cnt ) {
		numb_ov_cnt-- ;
		sema_up(&numb_overflow) ;
	}
	mu_unlock( &numb_mutex ) ;
}


/* The secondary set...
 * These procedure allow one to lock objects by giving their identifying
 * class and number. These procedures do not allow read/write locking.
 * only a single kind is available.
 */

/* This variable could be tuned. Setting it to too low numbers degrades
 * performance severely.
 */
#define NUMLOCKS	20	/* The maximum number of locks */

/* The lock for the whole following data structure */
static mutex data_lock ;

/* The lock used to wait for empty slots */
static semaphore ovfl_lock ;

/* The number of threads waiting for the overflow lock */
static int   ovfl_cnt ;

/* The actual lock array */
static struct single_lock {
	int	cnt ;		/* # threads using lock */
	int	lclass ;	/* lock class */
	long	number ;	/* number within class */
	mutex	lock ;		/* The actual lock */
} lock_list[NUMLOCKS];
	
/* Statistics */
int	     bs_max_locks_used ;
int	     bs_had_ovflw ;


/* Find an entry in the list.
 * This routine assumes that the data structure is locked. It also
 * returns with the structure locked. It might have been freed in the
 * meantime though!
 * Returns 0 when existing lock is not found. Keeps trying if no space for
 * new lock is found.
 */
static struct single_lock *
find_lock(lclass,number,new)
	int	lclass ;
	long	number ;
	int	new ;
{
	int	i ;
	register struct single_lock *sl ;
	int	empty_index ;

	empty_index= -1 ;
	for(;;) {
		for( i=0, sl=lock_list ; i<NUMLOCKS ; i++, sl++ ) {
			if ( sl->cnt==0 ) {
				if ( empty_index== -1 ) {
					empty_index= i ;
				}
			} else {
				if ( sl->lclass==lclass &&
				     sl->number==number ) {
					return  sl ;
				}
			}
		}
		/* No lock found ... */
		if ( !new ) return (struct single_lock *)0 ;
		if ( empty_index>bs_max_locks_used ) {
			bs_max_locks_used=empty_index ;
		}
		if ( empty_index>=0 ) {
			/* Found a spot for a new lock */
			sl= &lock_list[empty_index] ;
			sl->lclass= lclass ;
			sl->number= number ;
			return sl ;
		}
		/* No space for new lock, wait until an entry is freed */
		bs_had_ovflw++ ;
		ovfl_cnt++ ;
		mu_unlock(&data_lock) ; /* Free access to data */
		sema_down(&ovfl_lock) ; /* Wait .... */
		mu_lock(&data_lock) ; /* Get data lock and retry */
	}
}

void typed_lock_init() {
	sema_init(&ovfl_lock,0) ;
}

void typed_lock(lclass,number)
	int	lclass;
	long	number;
{
	register struct single_lock	*sl ;

	mu_lock(&data_lock) ;
	sl= find_lock(lclass,number,1) ;
	sl->cnt++ ;
	mu_unlock(&data_lock) ;
	mu_lock(&sl->lock) ;
	dbmessage(13,("lock success: class %d, # %ld",lclass,number));
}

int typed_trylock(lclass,number)
	int	lclass;
	long	number;
{
	register struct single_lock	*sl ;

	if ( mu_trylock(&data_lock,(interval)20) ) {
		/* Can't acquire lock of locks, abort attempt */
		dbmessage(3,("data lock timeout"));
		return -1 ;
	}
	sl= find_lock(lclass,number,1) ;
	/* We try without delay, thus freeing data_lock before trying
	 * is not necessary.
	 */
	if ( mu_trylock(&sl->lock,(interval)0) ) {
		mu_unlock(&data_lock) ;
		/* Failure, usage count is still 0 */
		return -1 ;
	}
	dbmessage(13,("trylock success: class %d, # %ld",lclass,number));
	sl->cnt++ ; /* Success, update usage count */
	mu_unlock(&data_lock) ;
	return 0 ;
}

void typed_unlock(lclass,number)
	int	lclass;
	long	number;
{
	register struct single_lock	*sl ;

	mu_lock(&data_lock) ;
	sl= find_lock(lclass,number,0) ;
	if ( sl ) {
		mu_unlock(&sl->lock);
		sl->cnt-- ;
		dbmessage(13,("unlock success: class %d, # %ld",lclass,number));
		if ( sl->cnt==0 && ovfl_cnt ) {
			/* There is one waiting for a free spot */
			ovfl_cnt-- ;
			sema_up(&ovfl_lock) ;
		}
	} else {
		bwarn("Unlocking non-existant lock, type %d, # %d",
			lclass,number);
	}
	mu_unlock(&data_lock) ;
}

int test_typed_lock(lclass,number)
	int	lclass;
	long	number;
{
	register struct single_lock	*sl ;

	mu_lock(&data_lock) ;
	sl= find_lock(lclass,number,0) ;
	mu_unlock(&data_lock) ;
	return sl!=0 ;
}
