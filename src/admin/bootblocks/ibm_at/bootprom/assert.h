/*	@(#)assert.h	1.2	94/04/06 11:31:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * assert.h
 */
#ifndef	NDEBUG
#define	assert(exp) \
	if (!(exp)) { \
	    printf("Assertion \"%s\" failed: file %s, line %d\n", \
		#exp, __FILE__, __LINE__); \
	    halt(); \
	}
#else
#define	assert(exp)	/* */
#endif /* _ASSERT_H */
