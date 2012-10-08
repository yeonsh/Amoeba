/*	@(#)stdlib.h	1.2	94/04/06 17:21:56 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __STDLIB_H__
#define __STDLIB_H__

#ifdef  _SIZE_T_                        /* SGI uses this one */
#define _SIZE_T
#endif  /* _SIZE_T */
#if     !defined(_SIZE_T)
#define _SIZE_T
#define _SIZE_T_
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

#if     !defined(_WCHAR_T)
#define _WCHAR_T
typedef char    wchar_t;
#endif  /* _WCHAR_T */

#include "../generic/stdlib_gen.h"

#endif /* __STDLIB_H__ */
