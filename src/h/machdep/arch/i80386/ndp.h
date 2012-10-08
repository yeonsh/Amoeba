/*	@(#)ndp.h	1.2	94/04/06 15:59:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __NDP_H__
#define __NDP_H__

/*
 * 80387/80486 co-processor definitions
 */

/* co-processor types */
#define	NDP_NONE	0	/* no co-processor available */
#define	NDP_387		387	/* a 387 chip is available */
#define	NDP_487		487	/* a 487 chip is available */

/* control word masks */
#define	NDP_INV		0x0001	/* invalid operation */
#define	NDP_DNO		0x0002	/* denormalized operand */
#define	NDP_ZDIV	0x0004	/* zero divide */
#define	NDP_OVR		0x0008	/* overflow */
#define	NDP_UNR		0x0010	/* underflow */
#define	NDP_PRE		0x0020	/* precision */
#define	NDP_PC		0x0300	/* precision control */
#define	NDP_RC		0x0C00	/* rounding control */
#define	NDP_IC		0x1000	/* infinity control */

/* precision, rounding, and infinity options in control word */
#define	NDP_SIG24	0x0000	/* 24-bit significand precision (short) */
#define	NDP_SIG53	0x0200	/* 53-bit significand precision (long) */
#define	NDP_SIG64	0x0300	/* 64-bit significand precision (temp) */
#define	NDP_RTN		0x0000	/* round to nearest or even */
#define	NDP_RD		0x0400	/* round down */
#define	NDP_RU		0x0800	/* round up */
#define	NDP_CHOP	0x0C00	/* chop (truncate toward zero) */
#define	NDP_P		0x0000	/* projective infinity */
#define	NDP_A		0x1000	/* affine infinity */

#endif /* __NDP_H__ */
