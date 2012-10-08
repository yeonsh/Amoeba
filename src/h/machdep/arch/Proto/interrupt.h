/*	@(#)interrupt.h	1.2	94/04/06 15:55:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

/*
 * typedef for information needed to set up an interrupt.
 * Can be used in various drivers. Currently only the Lance.
 */

typedef struct {
	long	probably_vector_and_other_info_as_needed;
} intrinfo;

#endif /* __INTERRUPT_H__ */
