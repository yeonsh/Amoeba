/*	@(#)lint.h	1.3	96/02/27 12:43:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	Declarations to keep lint's mouth shut.
 *	Too bad /usr/lib/lint/llib-lc differs from unix to unix.
 */
/*
 *	4.3 Tahoe is not POSIX conforming:
 */
# if tahoe
extern int sprintf();	/* Declare ourselves */
# define memcpy(to, from, n)	bcopy(from, to, n)
# endif

# define chkptr(p)	assert(!(((long)(p))&0x01L))

# ifdef lint
# undef assert
# define assert(e)
# endif /* lint */
