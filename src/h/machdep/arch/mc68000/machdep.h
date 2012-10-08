/*	@(#)machdep.h	1.3	94/04/06 16:00:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MACHDEP_H__
#define __MACHDEP_H__

#define CLICKSHIFT	13
#define CLICKSIZE	(1 << CLICKSHIFT)

#define VIR_LOW		0x0000	/* lowest virtual user click */
#define VIR_HIGH	0x1000	/* top virtual user click */

typedef unsigned long	vir_bytes;
typedef unsigned long	vir_clicks;
typedef unsigned long	phys_bytes;
typedef unsigned long	phys_clicks;

#endif /* __MACHDEP_H__ */
