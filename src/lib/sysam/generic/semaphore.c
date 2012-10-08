/*	@(#)semaphore.c	1.3	95/05/17 09:47:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Counting semaphores.  See semaphore.h. */

#include "amoeba.h"
#include "semaphore.h"
#include "module/mutex.h"

/* Initialize a semaphore.
   This call is compulsory, since an all-zero semaphore is impossible
   (when the level is zero, the lock must be set). */

void
sema_init(s, level)
	register semaphore *s;
	register int level;
{
#ifdef DEBUG
	if (level < 0)
		sema_abort("sema_init: level < 0"); /* Illegal call */
#endif /* DEBUG */
#ifndef KERNEL
	mu_init(&s->sema_mu);
#endif /* KERNEL */
	mu_init(&s->sema_lock);
	s->sema_lev = level;
	if (s->sema_lev == 0)
		mu_lock(&s->sema_lock);
}


/* Return the semaphore's level
   (not too useful -- it might change the next microsecond) */

int
sema_level(s)
	register semaphore *s;
{
	/* NB this assumes an aligned int memory reference is atomic */
	return s->sema_lev;
}


/* Push the semaphore's level one down; block while level == 0 */

void
sema_down(s)
	register semaphore *s;
{
	/* When the semaphore's level is zero, we block at the following
	   call until a call to sema_up is made in another thread */
	mu_lock(&s->sema_lock);
#ifndef KERNEL
	mu_lock(&s->sema_mu);
#endif /* KERNEL */
#ifdef DEBUG
	if (s->sema_lev == 0)
		sema_abort("sema_down: level==0"); /* Cannot happen */
#endif /* DEBUG */
	s->sema_lev--;
	if (s->sema_lev != 0)
		mu_unlock(&s->sema_lock);
#ifndef KERNEL
	mu_unlock(&s->sema_mu);
#endif /* KERNEL */
}

/* Same with time-out */

int
sema_trydown(s, maxdelay)
	register semaphore *s;
	interval maxdelay;
{
	int ret;
	
	ret = mu_trylock(&s->sema_lock, maxdelay);
	if (ret < 0)
		return ret;
#ifndef KERNEL
	mu_lock(&s->sema_mu);
#endif /* KERNEL */
#ifdef DEBUG
	if (s->sema_lev == 0)
		sema_abort("sema_trydown: level==0"); /* Cannot happen */
#endif /* DEBUG */
	s->sema_lev--;
	if (s->sema_lev != 0)
		mu_unlock(&s->sema_lock);
#ifndef KERNEL
	mu_unlock(&s->sema_mu);
#endif /* KERNEL */
	return 0;
}

/* Push the semaphore's level multiple down.
   Returns the actual decrement made to the semaphore;
   this may be less than the count argument but is >= 1 as long
   as the count argument is >= 1. */

int
sema_mdown(s, count)
	register semaphore *s;
	int count;
{
	if (count <= 0)
		return 0;
	/* When the semaphore's level is zero, we block at the following
	   call until a call to sema_up is made in another thread */
	mu_lock(&s->sema_lock);
#ifndef KERNEL
	mu_lock(&s->sema_mu);
#endif /* KERNEL */
#ifdef DEBUG
	if (s->sema_lev == 0)
		sema_abort("sema_mdown: level==0"); /* Cannot happen */
#endif /* DEBUG */
	if (s->sema_lev < count)
		count = s->sema_lev;
	s->sema_lev -= count;
	if (s->sema_lev != 0)
		mu_unlock(&s->sema_lock);
#ifndef KERNEL
	mu_unlock(&s->sema_mu);
#endif /* KERNEL */
	return count;
}

int
sema_trymdown(s, count, maxdelay)
	register semaphore *s;
	int count;
	interval maxdelay;
{
	int ret;

	if (count <= 0)
		return 0;
	/* When the semaphore's level is zero, we block at the following
	   call until a call to sema_up is made in another thread */
	if ((ret=mu_trylock(&s->sema_lock, maxdelay)) < 0 )
		return ret;
#ifndef KERNEL
	mu_lock(&s->sema_mu);
#endif /* KERNEL */
#ifdef DEBUG
	if (s->sema_lev == 0)
		sema_abort("sema_trymdown: level==0"); /* Cannot happen */
#endif /* DEBUG */
	if (s->sema_lev < count)
		count = s->sema_lev;
	s->sema_lev -= count;
	if (s->sema_lev != 0)
		mu_unlock(&s->sema_lock);
#ifndef KERNEL
	mu_unlock(&s->sema_mu);
#endif /* KERNEL */
	return count;
}

/* Pop the semaphore's level one up; awake blocked sema_*down calls */

void
sema_up(s)
	register semaphore *s;
{
#ifndef KERNEL
	mu_lock(&s->sema_mu);
#endif /* KERNEL */
	if (s->sema_lev == 0)
		mu_unlock(&s->sema_lock);
	s->sema_lev++;
#ifdef DEBUG
	if (s->sema_lev == 0)
		sema_abort("sema_up: level==0 afterwards"); /* Overflow */
#endif /* DEBUG */
#ifndef KERNEL
	mu_unlock(&s->sema_mu);
#endif /* KERNEL */
}

/* Pop the semaphore's level multiple up; awake blocked sema_*down calls.
   It is illegal to cause the level to overflow. */

void
sema_mup(s, count)
	register semaphore *s;
	int count;
{
	if (count <= 0)
		return;
#ifndef KERNEL
	mu_lock(&s->sema_mu);
#endif /* KERNEL */
	if (s->sema_lev == 0)
		mu_unlock(&s->sema_lock);
	s->sema_lev += count;
#ifdef DEBUG
	if (s->sema_lev <= 0)
		sema_abort("sema_mup: level<=0 afterwards"); /* Overflow */
#endif /* DEBUG */
#ifndef KERNEL
	mu_unlock(&s->sema_mu);
#endif /* KERNEL */
}

#ifdef DEBUG

/* Abort with error message */

static
sema_abort(s)
	char *s;
{
	puts(s);
	_exit(-1);
}

#endif /* DEBUG */
