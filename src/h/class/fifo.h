/*	@(#)fifo.h	1.2	94/04/06 15:49:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FIFO_H__
#define __FIFO_H__

#include <_ARGS.h>

/* Inherited classes: */
#include "bullet_rgts.h"

/* Operators: */
errstat fifo_create _ARGS((capability *_cap, /* out */ capability *fifo_cap));
errstat fifo_open   _ARGS((capability *_cap, /* in */ int flags,
			  /* out */ capability *fifo_handle));
errstat fifo_share  _ARGS((capability *_cap));
errstat fifo_close  _ARGS((capability *_cap));
errstat fifo_bufsize _ARGS((capability *_cap, /* out */	long int *size));

#endif /* __FIFO_H__ */
