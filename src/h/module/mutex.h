/*	@(#)mutex.h	1.3	96/02/27 10:33:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_MUTEX_H__
#define __MODULE_MUTEX_H__

#define	mu_init		_mu_init
#define	mu_lock		_mu_lock
#define	mu_trylock	_mu_trylock
#define	mu_unlock	_mu_unlock

void	mu_init _ARGS((mutex *));
void	mu_lock _ARGS((mutex *));
int	mu_trylock _ARGS((mutex *, interval));
void	mu_unlock _ARGS((mutex *));

#endif /* __MODULE_MUTEX_H__ */
