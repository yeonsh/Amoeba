/*	@(#)cputype.h	1.2	94/04/06 09:25:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __CPUTYPE_H__
#define __CPUTYPE_H__

#define	CPU_ARCH	0xf0		/* mask for architecture bits */
#define	ARCH_SUN3	0x10		/* architecture for Sun 3 */

#define	CPU_MACH	0x0f		/* mask for machine implementation */
#define	MACH_50		0x02
#define MACH_60		0x07
#define	MACH_110	0x04
#define	MACH_160	0x01
#define	MACH_260	0x03

#define SUN3_50		(ARCH_SUN3 | MACH_50)
#define SUN3_60		(ARCH_SUN3 | MACH_60)
#define SUN3_110	(ARCH_SUN3 | MACH_110)
#define SUN3_160	(ARCH_SUN3 | MACH_160)
#define SUN3_260	(ARCH_SUN3 | MACH_260)

#endif /* __CPUTYPE_H__ */
