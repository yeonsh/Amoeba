/*	@(#)memcmp.c	1.2	94/04/07 10:37:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** memcmp - compare up to first 'n' characters of arrays 's1' and 's2'. 
**	    Return lexicographic difference between s1 and s2.
*/

#include <string.h>

#ifndef __STDC__
#define const /*const*/
#endif

int
memcmp(s1, s2, n)
const _VOIDSTAR s1;
const _VOIDSTAR s2;
register size_t	n;
{
        register const char *p1 = (char *)s1, *p2 = (char *)s2;

        if (n) {
                n++;
                while (--n > 0) {
                        if (*p1++ == *p2++) continue;
                        return *--p1 - *--p2;
                }
        }
        return 0;
}
