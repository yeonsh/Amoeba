/*	@(#)malloctune.h	1.4	96/02/27 10:25:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MALLOCTUNE_H__
#define __MALLOCTUNE_H__

/*
** malloctune.h
**	Tunable parameters for malloc.
*/

/*
** The ALIGNMENT belongs in the compiler somewhere, but will have to live
** here for now.
*/

/* Alignment assumed by malloc for aligning memory requests */
#define ALIGN_SHIFT	3
#define	ALIGNMENT	(1 << ALIGN_SHIFT)

/* Number of free lists needed */
#define	NUM_CLASSES	16

/*
** Alignment size for segments.  New memory is allocated in multiples of
** SEGSIZE.  Should be the >= CLICKSIZE for the kernel.
** NB: it MUST be a power of 2.
*/
#define	MIN_SEGSIZE	(8*1024)

#include "machdep.h"
#if CLICKSIZE > MIN_SEGSIZE
#define	SEGSIZE	CLICKSIZE
#else
#define	SEGSIZE	MIN_SEGSIZE
#endif

#endif /* __MALLOCTUNE_H__ */
