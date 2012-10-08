/*	@(#)assert.h	1.5	94/04/06 17:17:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef NDEBUG

#define	INIT_ASSERT	/* NOTHING */
#define assert(e)	/* NOTHING */
#define assertN(n, e)	/* NOTHING */
#define compare(a,t,b)	/* NOTHING */

#else /*NDEBUG*/

/* are badassertion, badassertionN, ... the right names? Maybe they should be
   renamed to _badassertion, etc.
*/

#include <_ARGS.h>
void badassertion _ARGS(( char *file, int line ));
void badassertionN _ARGS(( int n, char *file, int line ));
void badcompare _ARGS(( char *file, int line, long v1, long v2 ));

#define	INIT_ASSERT	static char * ___FiLnAmE__ = __FILE__;

#define assert(e)	do if (!(e)) \
				badassertion(___FiLnAmE__, __LINE__); \
			while (0)

#define assertN(n, e)	do if (!(e)) \
				badassertionN(n, ___FiLnAmE__, __LINE__); \
			while (0)

#define compare(a,t,b)	do if (!((a) t (b))) \
				badcompare(___FiLnAmE__, __LINE__,	 \
						(long) (a), (long) (b)); \
			while (0)

#endif /*NDEBUG*/

#endif /* __ASSERT_H__ */
