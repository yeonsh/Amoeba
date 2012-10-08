/*	@(#)syscall.h	1.4	96/02/27 10:34:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
module/syscall.h

Function declarations for system calls

Created:	Jan 27, 1992 by Philip Homburg
*/

#ifndef _MODULE__SYSCALL_H_
#define _MODULE__SYSCALL_H_

#include <_ARGS.h>

#define	sys_null	_sys_null
#define	sys_milli	_sys_milli
#define	sys_cleanup	_sys_cleanup

unsigned long	sys_milli _ARGS(( void ));
void		sys_null _ARGS(( void ));
void		sys_cleanup _ARGS(( void ));

#endif /* _MODULE__SYSCALL_H_ */
