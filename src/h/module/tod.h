/*	@(#)tod.h	1.3	96/02/27 10:34:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_TOD_H__
#define __MODULE_TOD_H__

#define	tod_defcap	_tod_defcap
#define	tod_gettime	_tod_gettime
#define	tod_setcap	_tod_setcap
#define	tod_settime	_tod_settime

errstat		tod_defcap _ARGS((void));
errstat		tod_gettime _ARGS((long *, int *, int *, int *));
void		tod_setcap _ARGS((capability *));
errstat		tod_settime _ARGS((long, int));

#endif /* __MODULE_TOD_H__ */
