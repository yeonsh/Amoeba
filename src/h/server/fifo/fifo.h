/*	@(#)fifo.h	1.2	94/04/06 16:59:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FIFO_H__
#define __FIFO_H__

/* Command numbers */
#define FIFO_CREATE	(FIFO_FIRST_COM)
#define FIFO_OPEN	(FIFO_FIRST_COM + 1)
#define FIFO_SHARE	(FIFO_FIRST_COM + 2)
#define FIFO_CLOSE	(FIFO_FIRST_COM + 3)
#define FIFO_BUFSIZE	(FIFO_FIRST_COM + 4)

/* Fifo-specific error codes: */
#define FIFO_AGAIN	((errstat) (FIFO_FIRST_ERR - 1))
#define FIFO_BADF	((errstat) (FIFO_FIRST_ERR - 2))
#define FIFO_ENXIO	((errstat) (FIFO_FIRST_ERR - 3))

#include "_ARGS.h"

/* Operators: */
errstat fifo_create  _ARGS((capability *_cap, capability *fifo_cap));
errstat fifo_open    _ARGS((capability *_cap, int flags, capability *handle));
errstat fifo_share   _ARGS((capability *_cap));
errstat fifo_close   _ARGS((capability *_cap));
errstat fifo_bufsize _ARGS((capability *_cap, long int *size));

#endif /* __FIFO_H__ */
