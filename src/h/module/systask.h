/*	@(#)systask.h	1.7	96/02/27 10:34:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _MODULE_SYSTASK_H_
#define _MODULE_SYSTASK_H_

#include <stddef.h>

#define	sys_boot	_sys_boot
#define	sys_kstat	_sys_kstat
#define	sys_printbuf	_sys_printbuf
#define	sys_profile	_sys_profile

errstat sys_boot _ARGS((capability *, capability *, char *, int));
errstat sys_kstat _ARGS((capability *, char, char *, bufsize, int *));
errstat sys_printbuf _ARGS((capability *, char *, bufsize, int *, int *));
bufsize sys_profile _ARGS(( capability *cap, char *buf, size_t nbytes,
							unsigned long interv ));

#endif /* _MODULE_SYSTASK_H_ */
