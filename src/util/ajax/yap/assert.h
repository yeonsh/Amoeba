/*	@(#)assert.h	1.1	91/04/12 14:57:25 */
/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* Assertion macro */

# ifndef _ASSERT_
# define PUBLIC extern
# else
# define PUBLIC
# endif

#ifdef DO_ASSERT
#define assert(x) if(!(x)) badassertion("x",__FILE__,__LINE__)
VOID badassertion();
/*
 * void badassertion(ass,fn,lineno)
 * char *ass,		The assertion in string form,
 *	*fn;		The filename in which the assertion failed,
 * int lineno;		The line number of the assertion.
 *
 * Reports the assertion on standard output and then aborts with a core dump.
 */
#else
#define assert(x)	/* nothing */
#endif

# undef PUBLIC
