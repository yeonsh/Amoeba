/*	@(#)promid_sun4.h	1.5	94/05/17 10:27:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef	__PROMID_SUN4_H__
#define	__PROMID_SUN4_H__

/* The id prom in a sun4c has the following structure
 *
 *	char format (always 1 as far as I can tell)
 *      char machine-id (one of the constants below)
 *	char ether_addr[6]
 *      long manufacture-date (usually all set to 0xA5)
 *      char serial#[3]
 *      char xor-checksum
 *      char reserved[16] (mostly set to 0xA5 !!?)
 *
 * Below are the known machine id's.  The high order nibble gives
 * architecture type (eg. 2 => sun4, 5 => sun4c) and the low order
 * nibble designates the machine type.
 */

#define CPU_ARCH	0xF0		/* Mask for architecture */
#define CPU_MACH	0x0F		/* Mask for machine */

#define ARCH_SUN4	0x20
#define MACH_SUN4_260	0x21
#define MACH_SUN4_110	0x22
#define MACH_SUN4_330	0x23
#define MACH_SUN4_460	0x24

#define ARCH_SUN4C	0x50
#define MACH_SUN4C_60	0x51		/* SparcStation 1 */
#define MACH_SUN4C_40	0x52		/* IPC */
#define MACH_SUN4C_65	0x53		/* SparcStation 1+ */
#define MACH_SUN4C_20	0x54		/* SLC */
#define MACH_SUN4C_75	0x55		/* Sparcstation 2 */
#define MACH_SUN4C_30	0x56		/* ELC */
#define MACH_SUN4C_50	0x57		/* IPX */
#define MACH_SUN4C_70	0x58
#define MACH_SUN4C_80	0x59
#define MACH_SUN4C_10	0x5a
#define MACH_SUN4C_45	0x5b
#define MACH_SUN4C_05	0x5c
#define MACH_SUN4C_85	0x5d
#define MACH_SUN4C_32	0x5e
#define MACH_SUN4C_XX	0x5f

/*
 * This structure describes all the sparc implementation dependant parameters,
 * as well as a few processor or MMU specific items item thrown in just to
 * confuse matters.  The configuration byte is taken from the id prom on
 * Suns but if there is some other sort of sparc system that has no id prom
 * we'll have to invent something to identify them.
 */
struct mach_info {
    int		mi_id;			/* Configuration byte */
    int		mi_nwindows;		/* Number of windows */
    int		mi_contexts;		/* Number of contexts */
    int		mi_pmegs;		/* Number of PMEGs */
    int		mi_cpu_delay;		/* Loop count for microsec delay */
};
extern struct mach_info machine;	/* Our copy of the machine info */

#endif	/* __PROMID_SUN4_H__ */
