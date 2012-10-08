/*	@(#)__stdarg.h	1.1	96/02/27 10:40:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef	__STD_ARG_H__
#define	__STD_ARG_H__

#ifdef __sparc__

#include "../generic/__stdarg.h"

#else

typedef	void *	___va_list;

#endif /* __sparc__ */

#endif /* __STD_ARG_H__ */
