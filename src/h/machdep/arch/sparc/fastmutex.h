/*	@(#)fastmutex.h	1.1	96/02/27 10:30:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_MUTEX_H__
#define __MODULE_MUTEX_H__

#define	mu_init		_mu_init
#define	mu_lock		_mu_lock
#define	mu_unlock	_mu_unlock
#define	mu_trylock	_mu_trylock
void	mu_init _ARGS((mutex *));

#if !(defined(sparc) || defined(__sparc__))

void	mu_lock _ARGS((mutex *));
int	mu_trylock _ARGS((mutex *, interval));
void	mu_unlock _ARGS((mutex *));

#else

#define MU_RESERVE_BYTE(mu)      ((volatile char *) mu)
#define MU_FASTLOCK(mu)         (((volatile char *) mu) + 1)

extern void _sys_mu_unlock _ARGS(( mutex * ));
extern int _sys_mu_trylock _ARGS(( mutex *, interval ));

#ifdef __GNUC__
#define fast_asm_ldstub(mu, lock) \
	__asm__ volatile("ldstub [%1 + 1], %0" : "=r" (lock) : "r" (mu) : "memory")
#define flush_to_memory() \
	__asm__ volatile("" : : : "memory")
#else
#define fast_asm_ldstub(mu, lock) \
	{ lock = __ldstub(MU_FASTLOCK(mu)); }
#define flush_to_memory()
#endif

#ifdef __GNUC__
__inline__
#endif
static
void _mu_lock(register mutex *mu)
    {
	register long lock;

	fast_asm_ldstub(mu, lock);
	while (lock != 0) {
	    lock = _sys_mu_trylock(mu,-1L);
	}
    }

#ifdef __GNUC__
__inline__
#endif
static
int _mu_trylock(register mutex *mu, interval t)
    {
	register long lock;

	fast_asm_ldstub(mu, lock);
	if (lock != 0) {
	    return _sys_mu_trylock(mu,t);
	}
	else
	    return 0;
    }

#ifdef __GNUC__
__inline__
#endif
static
void _mu_unlock(register mutex *mu)
    {
	register long lock;

	flush_to_memory();
	*MU_FASTLOCK(mu) = 0;
	if (*MU_RESERVE_BYTE(mu) != 0){ 
	    fast_asm_ldstub(mu, lock);
	    if (lock == 0) {
	        _sys_mu_unlock(mu);
	    }
	}
    }

#endif /* sparc || __sparc__ */
#endif /* __MODULE_MUTEX_H__ */
