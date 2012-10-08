/*	@(#)unistand.h	1.2	94/04/07 14:55:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Include unistd.h if it is available.
 * Otherwise we define the macros needed ourselves.
 */
#if defined(_POSIX_SOURCE) || defined(_MINIX) || defined(AMOEBA)
#include <sys/types.h>
#include <unistd.h>
#endif

/* Arguments for access: */
#ifndef F_OK
#define F_OK	0
#endif
#ifndef W_OK
#define W_OK	2
#endif
#ifndef R_OK
#define R_OK	4
#endif
