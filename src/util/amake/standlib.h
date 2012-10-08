/*	@(#)standlib.h	1.2	94/04/07 14:54:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#if defined(__STDC__) || defined(sun) || defined(AMOEBA) || defined(_MINIX) || defined(SYSV)
#include <stdlib.h>
#else
/* Not sure there is a <stdlib.h>; declare the ones we use */
extern long atol();
extern char *malloc();
extern char *realloc();
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE	1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS	0
#endif
