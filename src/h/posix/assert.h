/*	@(#)assert.h	1.2	94/04/06 16:52:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ASSERT_H__
#define __ASSERT_H__

#undef  assert

/*
** Assertion statement, disappears when NDEBUG is #defined.
*/

#if     defined(NDEBUG)
#define assert(ignore)  ((void)0)	/* definition forced by ANSI */
#else

#ifdef __STDC__

void __bad_assertion(const char *_mess);

#define __str(x)        # x
#define __xstr(x)       __str(x)

#define assert(expr)    ((expr)? (void)0 : \
                                __bad_assertion("Assertion \"" #expr \
                                    "\" failed, file " __xstr(__FILE__) \
                                    ", line " __xstr(__LINE__) "\n"))
#else /* !__STDC__ */

void __bad_assertion(/* char *file, int line */);

/* Some compilers can't handle an assertion expression like above,
 * so we'll have to make it an if statement.
 */
#define assert(expr)    if (!(expr)) {\
				__bad_assertion(__FILE__, __LINE__);\
			}

#endif /* __STDC__ */

#endif /* NDEBUG */

#endif /* __ASSERT_H__ */
