/*	@(#)interrupt.h	1.2	94/04/06 15:59:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

/*
 * Interrupt information (for now only used in glance.c)
 */
typedef struct {
    int		ii_irq;		/* interrupt request level */
    int		ii_dma;		/* dma channel number */
} intrinfo;

#endif /* __INTERRUPT_H__ */
