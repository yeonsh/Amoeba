/*	@(#)stddef.h	1.4	94/04/06 16:54:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** Standard definitions as required by ANSI C.
**
** XXX Almost all of this header file is compiler-dependent; however,
** these definitions do nicely for the class of machines we care about.
*/

#ifndef __STDDEF_H__
#define __STDDEF_H__

#ifdef __STDC__

#ifndef NULL
#define NULL            ((void *)0)
#endif

#ifdef _ACK
#if	_EM_PSIZE == _EM_WSIZE
typedef int     ptrdiff_t;      /* result of substracting two pointers */
 #elif	_EM_PSIZE == _EM_LSIZE
typedef long    ptrdiff_t;      /* result of substracting two pointers */
#else
 #error garbage pointer size
#endif  /* _EM_PSIZE */
#else
typedef int ptrdiff_t;
#endif  /* _ACK */

#else

/*
** A NULL pointer.
** Also at numerous other places, e.g., <stdio.h>.
*/
#ifndef NULL
#define	NULL	0
#endif

/*
** Type returned by pointer subtraction.
** Also in Posix <sys/types.h>.
*/
#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef int ptrdiff_t;
#endif

#endif /* __STDC__ */

#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

#if     !defined(_WCHAR_T)
#define _WCHAR_T
typedef char    wchar_t;                /* type expanded character set */
#endif  /* _WCHAR_T */

/*
** Offset of member in struct.
** XXX A portable definition is impossible.
*/
#define offsetof(type, ident)           ((size_t) &(((type *)0)->ident))

#endif /* __STDDEF_H__ */
