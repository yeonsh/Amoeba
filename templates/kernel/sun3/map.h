/*	@(#)map.h	1.5	96/02/16 15:47:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** map.h
**	memory mapping constants
*/

/*
 * The sun3 can't support more than 8 user processes + the kernel so we
 * restrict NPROC here
 */
#ifndef	NPROC
#define	NPROC		9
#endif

#if NPROC > 9
You cannot set NPROC to be more than 9
#endif

#define IVECBASE	0x0F000000		/* start of int vectors */

#define KERNELSTART	((vir_bytes) IVECBASE)	/* kernel starts here */
#define KERNELTOP	((vir_bytes) KERNELSTART + 0x100000)

#define IOBASE		((vir_bytes) 0x8000000)	/* io space begins here */
#define BMBASE		(0xE000000)		/* bit map mapped here */

#define	BUSADDR(x)	((phys_bytes)(x) - IVECBASE)

#define PB_START	((struct printbuf *) (IVECBASE + 0x400))
#define PB_SIZE		0x1c00

#define PTOV(x)		((vir_bytes) (x))
#define VTOP(x)		((phys_bytes) (x))
