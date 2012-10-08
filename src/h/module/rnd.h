/*	@(#)rnd.h	1.3	96/02/27 10:33:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_RND_H__
#define __MODULE_RND_H__

#define	rnd_setcap	_rnd_setcap
#define	rnd_defcap	_rnd_defcap
#define rnd_getrandom	_rnd_getrandom
#define	uniqport	_uniqport

void	rnd_setcap    _ARGS((capability *));
errstat	rnd_defcap    _ARGS((void));
errstat	rnd_getrandom _ARGS((char *, int));
void	uniqport      _ARGS((port *));

#endif /* __MODULE_RND_H__ */
