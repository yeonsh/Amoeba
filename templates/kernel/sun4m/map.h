/*	@(#)map.h	1.3	96/02/16 15:53:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: map.h -- configuration constants
 */

#ifndef _MAP_H
#define _MAP_H

/* FLIP buffer parameters */
#ifndef FLIP_npkt
#define	FLIP_npkt	400
#endif /* FLIP_npkt */

#ifndef FLIP_grp_npkt
#define	FLIP_grp_npkt	200
#endif /* FLIP_grp_npkt */

/* Ethernet buffering parameters */
#define	NOVERFLOW		256
#define LOGNTRANS		4    	/* Define here even if defaults */
#define LOGNRECV		7

#define BIT( x )	( 1 << (x))

#define KERNELSTART	((vir_bytes) KERNBASE)	/* kernel starts here */

/*
 * VTOP() and PTOV() -- convert virtual address to physical address and
 * back. For the sparcstation, with it's discontinuous memory, we have to
 * fake a continuous address space for these macros. Real physical addresses
 * are only used be truly machine dependant code.
 */
#define	VTOP(x)		((phys_bytes) (x))
#define	PTOV(x)		((vir_bytes) (x))

/*
 * The following routines are not machine dependant, but we must still
 * provide them, just in case we wanted to do something special.These
 * are provided as routines in mm.h, for callers which do not include this
 * file (at the cost of a procedure call).
 */

#define phys_copy( src, dst, cnt )	\
    (void) memmove( (_VOIDSTAR) PTOV( dst ), (_VOIDSTAR) PTOV( src ), \
			(size_t) cnt )
#define phys_probe( p )			\
    probe( PTOV( p ))
#define phys_zero( dst, cnt )		\
    (void) memset( (_VOIDSTAR) PTOV( dst ), 0, (size_t) cnt )


/* See also VIR_LOW and VIR_HIGH from machdep.h */
#define USERLOW_SHIFT		MMU_SEGSHIFT
#define USERHIGH_SHIFT		29
#define USERLOW			(1<<USERLOW_SHIFT)
#define USERHIGH		(1<<USERHIGH_SHIFT)

#define KERNBASE		(0xE0000000)
#define IVECBASE		KERNBASE
#define KERNLOAD		(0x00004000+KERNBASE) /* just after printbuf */

#define PB_START		((struct printbuf *) (KERNBASE+0x1000))
#define PB_SIZE			0x3000	/* Make a larger print buffer */

#define KERNELSTART	((vir_bytes) KERNBASE)	/* kernel starts here */

#endif	/* _MAP_H */
