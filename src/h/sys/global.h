/*	@(#)global.h	1.3	94/04/06 17:17:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

typedef uint16		address;

#define NOWHERE		((address) 0)
#define SOMEWHERE	((address) -1)

#define ABSPTR(t, c)	((t) (c))

#define lobyte(x)	((uint16) (x) & 0xFF)
#define hibyte(x)	((uint16) (x) >> 8)
#define concat(x, y)	((uint16) (x) << 8 | (uint16) (y) & 0xFF)

#endif /* __GLOBAL_H__ */
