/*	@(#)bsearch.c	1.1	91/04/10 10:36:25 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <stdlib.h>
#include "ansi_compat.h"

_VOIDSTAR
bsearch(key, base, nmemb, size, compar)
register const _VOIDSTAR key;
register const _VOIDSTAR base;
register size_t nmemb;
register size_t size;
int (*compar) _ARGS((const void *, const void *));
{
	register const _VOIDSTAR mid_point;
	register int  cmp;

	while (nmemb > 0) {
		mid_point = (_VOIDSTAR) ((char *)base + size * (nmemb >> 1));
		if ((cmp = (*compar)(key, mid_point)) == 0)
			return (_VOIDSTAR)mid_point;
		if (cmp >= 0) {
			base  = (_VOIDSTAR) ((char *)mid_point + size);
			nmemb = (nmemb - 1) >> 1;
		} else
			nmemb >>= 1;
	}
	return (_VOIDSTAR)NULL;
}
