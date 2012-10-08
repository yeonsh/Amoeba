/*	@(#)alloca.h	1.2	94/04/06 16:55:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ALLOCA_H
#define __ALLOCA_H

#if defined(sparc) || defined(__sparc__)
# define alloca(x) __builtin_alloca(x)
#endif

#endif /* __ALLOCA_H */
