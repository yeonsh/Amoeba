/*	@(#)byte_order.h	1.3	94/04/06 11:23:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* See which endian we are by including byteorder.h */
#include "byteorder.h"

#ifdef __BIG_ENDIAN_H__
/* big_endian.h got included, so we're a big endian */
#define BYTE_ORDER	0x3210
#else
#ifdef __LITTLE_ENDIAN_H__
/* littl_endian.h got included, so we're a little endian */
#define BYTE_ORDER	0x0123
#else
	error: unknown endianness ?!
#endif /* __LITTLE_ENDIAN_H__ */
#endif /* __BIG_ENDIAN_H__ */
