/*	@(#)randseed.h	1.2	94/04/06 17:19:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __RANDSEED_H__
#define __RANDSEED_H__

#ifndef NORANDOM
/*
 * Interface to randseed() function
 * Supplies bits to seed random with
 *
 * Three types of bits can be given:
 * Different per host, but the same across invocations,
 * different per invocation, eg time dependent,
 * really random(hardware generated)
 *
 * If only we could find those last sort ...
 */

void randseed(/* value, nbits_valid, type */);

#define RANDSEED_HOST	0
#define RANDSEED_INVOC	1
#define RANDSEED_RANDOM	2

#define RANDSEED_NTYPES (RANDSEED_RANDOM+1)

#else /* NORANDOM */

#define randseed(v, n, t)	/* nothing */

#endif /* NORANDOM */

#endif /* __RANDSEED_H__ */
