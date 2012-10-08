/*	@(#)uart2681.h	1.2	94/04/06 16:46:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __UART2681_H__
#define __UART2681_H__

/*
 * parameters for Signetics 2681 Duart/timer chip
 */

extern
struct ua2681info {
	vir_bytes uai_devaddr;
	short     uai_vector;
} ua2681info;

#endif /* __UART2681_H__ */
