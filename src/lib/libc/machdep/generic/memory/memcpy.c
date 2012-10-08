/*	@(#)memcpy.c	1.2	94/04/07 10:37:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** memcpy - block copy of 'n' bytes from 'src' to 'dst'.  Returns 'dst'.  Must
**	    handle overlap of 'dst' and 'src' correctly.  We should copy
**	    backwards if 'src' is lower in memory than 'dst'.
*/

#include <string.h>

#ifndef __STDC__
#define const /*const*/
#endif

_VOIDSTAR
memcpy(dst, src, n)
_VOIDSTAR	dst;
const _VOIDSTAR	src;
size_t		n;
{
    return memmove(dst, src, n);
}
