/*	@(#)machdep.h	1.3	94/04/06 15:55:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MACHDEP_H__
#define __MACHDEP_H__

/*
 * Page size as seen from higher layers. Need not actually be anything
 * the hardware provides, but will be the unit of memory allocation
 * and information between machine independent, and machine dependent
 * code.
 */

#define CLICKSHIFT	13
#define CLICKSIZE	(1 << CLICKSHIFT)

/*
 * Lowest and highest address a user process can reach.
 * Unit is clicks(pages).
 */
#define VIR_LOW		0x0000	/* lowest virtual user click */
#define VIR_HIGH	0x1000	/* top virtual user click */

/*
 * Various integer typedefs for variables containing virtual and physical
 * sizes, in bytes and in clicks.  They *must* be unsigned types!!
 */

typedef unsigned long	vir_bytes;
typedef unsigned long	vir_clicks;
typedef unsigned long	phys_bytes;
typedef unsigned long	phys_clicks;

#endif /* __MACHDEP_H__ */
