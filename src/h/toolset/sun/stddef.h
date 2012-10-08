/*	@(#)stddef.h	1.2	94/04/06 17:22:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
stddef.h
*/

#ifndef	__stddef_h
#define	__stddef_h

#include <sys/stdtypes.h>

#ifndef NULL
#define	NULL		0
#endif

#define offsetof(type, ident)		((size_t) &(((type *)0)->ident))

#endif /* __stddef_h */
