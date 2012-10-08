/*	@(#)alloc.c	1.2	96/02/27 13:05:29 */
#include "global.h"
#include "alloc.h"
#include "standlib.h"
#include "error.h"

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

PRIVATE void
No_Mem()
{
        FatalError("Out of memory");
}

PUBLIC char *
Malloc(sz)
        unsigned int sz;
{
        register char *res = (char *)malloc(sz);

        if (sz > 0 && res == 0) No_Mem();
        return res;
}

PUBLIC char *
Realloc(ptr, sz)
        char *ptr;
        unsigned int sz;
{
        register char *mptr;

        if (!ptr) mptr = (char *)malloc(sz);
        else mptr = (char *)realloc(ptr, sz);
        if (sz > 0 && mptr == 0) No_Mem();
        return mptr;
}

PUBLIC char *
Salloc(str, sz)
        register char *str;
        register unsigned int sz;
{
        /*      Salloc() is not a primitive function: it just allocates a
                piece of storage and copies a given string into it.
        */
        char *res = (char *)malloc(sz);
        register char *m = res;

        if (sz > 0 && m == 0) No_Mem();
        while (sz--)
                *m++ = *str++;
        return res;
}

PUBLIC void
clear(ptr, n)
	register char *ptr;
	register unsigned int n;
{
	register long *q = (long *) ptr;

	while (n >= 8*sizeof (long))	{
			/* high-speed clear loop */
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		n -= 8*sizeof (long);
	}
	while (n >= sizeof (long))	{
			/* high-speed clear loop */
		*q++ = 0;
		n -= sizeof (long);
	}
	ptr = (char *) q;
	while (n--) *ptr++ = '\0';
}

PUBLIC char *
st_alloc(phead, size, count)
	char **phead;
	register unsigned int size;
	int count;
{
	register char *p;

	if (*phead == 0)	{
		while (count >= 1 && (p = (char *)malloc(size * count)) == 0) {
			count >>= 1;
		}
		if (p == 0) {
			No_Mem();
		}
		((PALLOC_) p)->_A_next = 0;
		while (--count) {
			p += size;
			((PALLOC_) p)->_A_next = (PALLOC_) (p - size);
		}
		*phead = p;
	}
	else	p = *phead;
	*phead = (char *) (((PALLOC_)p)->_A_next);

	clear(p, size);
	return p;
}

#ifdef DEBUG
PUBLIC char *
std_alloc(phead, size, count, total)
	char **phead;
	register unsigned int size;
	int count;
	int *total;
{
	if (*phead == 0)
		*total += count;
	return st_alloc(phead, size, count);
}
#endif
