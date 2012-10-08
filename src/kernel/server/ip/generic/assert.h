/*	@(#)assert.h	1.4	96/02/27 14:15:36 */
/*
assert.h

Copyright 1995 Philip Homburg
*/
#ifndef INET_ASSERT_H
#define INET_ASSERT_H

#include "const.h"

#ifndef assert 
#if NDEBUG
#define assert(x)		0
#define assertN(n,x)		0
#define compare(a,t,b)		0
#define compareN(n,a,t,b)	0
#else
#define assert(x) (!(x) ? (ip_panic (( "assertion failed" )) \
		,0) : 0)
#define assertN(n,x) (!(x) ? (ip_panic (( "assertion %d failed", n )),0) : 0)
#define compare(a,t,b) (!((a) t (b)) ? (ip_panic(( "compare failed (%d, %d)", \
	a, b )),0) : 0)
#define compareN(n,a,t,b) (!((a) t (b)) ? (ip_panic(( \
	"compare %d failed (%d, %d)", (n), (a), (b) )),0) : 0)
#endif
#endif

#endif /* INET_ASSERT_H */


/*
 * $PchId: assert.h,v 1.4 1995/11/21 06:45:27 philip Exp $
 */
