/*	@(#)grp_defs.h	1.1	96/02/27 14:07:21 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* A flag used to distinguish request through the private port
  from request to the public `bullet server` port.
*/

#define		BS_PEER		1	/* Peer request */
#define		BS_PUBLIC	0	/* Public request */

/* Group send Q flags, can be or'ed */
#define SEND_CANWAIT	0	/* Opposite of NOWAIT */
#define SEND_NOWAIT	1
#define	SEND_PRIORITY	2

/* Flags to writeinode regarding publishing of said inode */
#define ANN_NEVER	0	/* Don't publish */
#define ANN_MINE	1	/* Publish only if my own */
#define ANN_ALL		2	/* Must publish */
#define ANN_MASK	3	/* Mask for the above */

#define ANN_DELAY	0	/* Opposite of NOW */
#define ANN_NOW		4	/* do send i.s.o. publish */
