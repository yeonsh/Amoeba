/*	@(#)byteorder.h	1.2	94/04/06 15:55:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __BYTEORDER_H__
#define __BYTEORDER_H__

/*
 * Depending on the architecture either 
 * #include "byteorder/littl_endian.h"
 * or
 */
#include "byteorder/big_endian.h"

/*
 * Very wierd architectures might need something special,
 * for example the PDP 11 mixed endian long.
 */

#endif /* __BYTEORDER_H__ */
