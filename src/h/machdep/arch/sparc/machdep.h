/*	@(#)machdep.h	1.5	94/04/06 16:41:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MACHDEP_H__
#define __MACHDEP_H__

#define CLICKSHIFT	16
#define CLICKSIZE	(1<<CLICKSHIFT)

/*
 * Warning: these are page count equivalents of the values USERLOW
 * and USERHIGH in map.h. Care great care when changing them. These
 * cannot be calculated as they must be constants and our pagesize
 * is variable.
 */
#define VIR_LOW		0x0004		/* lowest virtual user click */
#define VIR_HIGH	0x2000		/* top virtual user click */

#ifndef _ASM

typedef unsigned long vir_bytes;
typedef unsigned long vir_clicks;
typedef unsigned long phys_bytes;
typedef unsigned long phys_clicks;

#endif	/* _ASM */

#define TBR_MASK	0xFF0		/* Mask out important bits of %tbr */
#define TBR_SHIFT	4		/* And shift to get regular # */

#define KTHREAD_STKSIZ	  8192		/* default of 4096 is too small */
#define KSTK_DEFAULT_SIZE 8192		/* ditto */

#endif /* __MACHDEP_H__ */
