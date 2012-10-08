/*	@(#)getblk.c	1.6	96/02/27 11:11:30 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * GETBLK
 *	Get a block of memory for malloc/free.
 *	May be called concurrently from multiple threads.
 *
 * Author: Gregory J. Sharp, June 1994 (derived from previous CWI version)
 */

#include "amoeba.h"
#include "module/proc.h"
#include "malloctune.h"

#define	MINSIZE		SEGSIZE
#define	NILCAP		((capability *) 0)


static int
newseg(addr, len)
long	addr;
long	len;
{
    segid	id;

    /*
     * First try to create a segment of at least MINSIZE bytes,
     * so we have some breathing space for the next allocations.
     * If that doesn't work, try a size of len bytes (if different).
     */
    id = seg_map(NILCAP, addr, (len < MINSIZE) ? MINSIZE : len,
				MAP_GROWUP | MAP_TYPEDATA | MAP_READWRITE);
    if (id < 0 && len < MINSIZE)
    {
	id = seg_map(NILCAP, addr, len,
				MAP_GROWUP | MAP_TYPEDATA | MAP_READWRITE);
    }
    return id;
}


char *
_getblk(incr)
unsigned int	incr;
{
    long	ret;

    ret = (long) findhole(incr < MINSIZE ? MINSIZE : (long) incr);
    if (ret != 0 && newseg(ret, (long) incr) < 0)
	ret = 0;
    return (char *) ret;
}
