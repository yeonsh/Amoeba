/*	@(#)uart8530info.h	1.2	94/04/06 16:46:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __UART8530INFO_H__
#define __UART8530INFO_H__

/*
 * parameters for Zilog? 8530 DUART
 */

extern
struct uart8530info {
	vir_bytes uai_devaddr;
	short     uai_vector;
} uart8530info;

#endif /* __UART8530INFO_H__ */
