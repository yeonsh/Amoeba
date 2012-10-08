/*	@(#)stdcmd.h	1.4	96/02/27 10:33:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_STDCMD_H__
#define __MODULE_STDCMD_H__

#define	std_age		_std_age
#define	std_copy	_std_copy
#define	std_destroy	_std_destroy
#define	std_getparams	_std_getparams
#define	std_info	_std_info
#define	std_restrict	_std_restrict
#define	std_setparams	_std_setparams
#define	std_status	_std_status
#define	std_touch	_std_touch
#define	std_ntouch	_std_ntouch

errstat	std_age _ARGS((capability *));
errstat std_copy _ARGS((capability *, capability *, capability *));
errstat	std_destroy _ARGS((capability *));
errstat	std_getparams _ARGS((capability *, char *, int, int *, int *));
errstat	std_info _ARGS((capability *, char *, int, int *));
errstat std_restrict _ARGS((capability *, rights_bits, capability *));
errstat	std_setparams _ARGS((capability *, char *, int, int));
errstat std_status _ARGS((capability *, char *, int, int *));
errstat	std_touch _ARGS((capability *));
errstat	std_ntouch _ARGS((port *, int, private *, int *));

#endif /* __MODULE_STDCMD_H__ */
