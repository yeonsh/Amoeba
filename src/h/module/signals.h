/*	@(#)signals.h	1.4	96/02/27 10:33:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_SIGNALS_H__
#define __MODULE_SIGNALS_H__

#include "exception.h"
#include "fault.h"

#define	sig_uniq	_sig_uniq
#define	sig_block	_sig_block
#define	sig_unblock	_sig_unblock
#define	sig_raise	_sig_raise
#define	sig_mu_lock	_sig_mu_lock
#define	sig_mu_trylock	_sig_mu_trylock
#define	sig_mu_unlock	_sig_mu_unlock
#define	sig_catch	_sig_catch

signum	sig_uniq	_ARGS((void));
void	sig_block	_ARGS((void));
void	sig_unblock	_ARGS((void));
void	sig_raise	_ARGS((signum));
void	sig_mu_lock	_ARGS((mutex *));
int	sig_mu_trylock	_ARGS((mutex *, interval));
void	sig_mu_unlock	_ARGS((mutex *));

errstat sig_catch 	_ARGS((
	signum _sig,
	void (*catcher)(signum _sig, thread_ustate *_us, void *_extra),
	void *_extra
));

/*
 * The following you probably should never use but they are here for
 * name-space pollution reduction reasons.
 */

#define	sys_sigret	_sys_sigret
#define	sys_setvec	_sys_setvec

void	sys_sigret _ARGS(( thread_ustate * ));
void	sys_setvec _ARGS(( sig_vector *, int ));

#endif /* __MODULE_SIGNALS_H__ */
