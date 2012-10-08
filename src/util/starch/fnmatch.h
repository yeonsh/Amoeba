/*	@(#)fnmatch.h	1.2	94/04/07 16:06:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Written by Guido van Rossum (guido@cwi.nl).
 * NO WARRANTIES!
 */

/* Flags for fnmatch(), or-ed together */
#define FNM_PATHNAME	0x0001	/* Match pathnames, not filenames */

/* Non-standard flags, recognized by this implementation: */
#define FNM_QUOTE	0x0100	/* Quote special chars with \ */
#ifndef _POSIX_SOURCE
#define FNM_HARMONICA	0x0200	/* ** matches multiple pathname components */
#endif
