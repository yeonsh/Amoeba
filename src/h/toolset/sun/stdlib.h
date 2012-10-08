/*	@(#)stdlib.h	1.5	94/04/06 17:22:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STDLIB_H__
#define __STDLIB_H__

#if !defined(__sys_stdtypes_h) && !defined(TYPES_H) && !defined(_TYPES_)
#define __sys_stdtypes_h	/* prevent multiple inclusion (SunOs 4.1) */

#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

#if     !defined(_WCHAR_T)
#define _WCHAR_T
typedef char    wchar_t;
#endif  /* _WCHAR_T */

typedef int sigset_t;			/* signal mask */
typedef int pid_t;			/* process id */
typedef long time_t;			/* process id */

#endif /* __sys_stdtypes_h */

#include "../generic/stdlib_gen.h"

#endif /* __STDLIB_H__ */
