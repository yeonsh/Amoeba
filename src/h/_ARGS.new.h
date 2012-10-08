/*	@(#)_ARGS.new.h	1.1	94/04/06 15:37:48 */
#ifndef ___ARGS_H__
#define ___ARGS_H__

/* This file defines macros for declarations and definitions of functions
 * that can be compiled by both ANSI-C compilers and traditional C compilers.
 *
 * Declarations should use the _ARGS macro:
 * int f _ARGS(( int x, short y ));
 * void g _ARGS(( void ));
 *
 * Definitions should use the _DEFUN, and _AND, or _DEFUN_VOID macros:
 * _DEFUN
 * (int f, (x, y),
 *	int x _AND
 *	short y
 * )
 * _DEFUN_VOID
 * (void g)
 */

#ifndef _ARGS

#ifdef __STDC__
#define _ARGS(x) x
#define _DEFUN(name, argslist, args)	name (args)
#define _AND	,
#define _DEFUN_VOID(name)		name (void)
#else
#define _ARGS(x) ()
#define _DEFUN(name, argslist, args)	name argslist args ;
#define _AND	;
#define _DEFUN_VOID(name)		name ()
#endif /* __STDC__ */

#endif /* _ARGS */

#endif /* ___ARGS_H__ */
