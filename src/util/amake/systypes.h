/*	@(#)systypes.h	1.4	94/04/07 14:54:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#if (defined(AMOEBA) || !defined(ACK)) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE   /* to get mode_t etc. */
#endif
#include <sys/types.h>
