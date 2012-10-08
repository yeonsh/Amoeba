/*	@(#)fsr.h	1.2	94/04/06 16:40:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FSR_H__
#define __FSR_H__

/*
 * FILE: fsr.h -- bits in the sparc floating-point state register
 *
 * Author: Greg Sharp
 */

#define	FSR_RD		0xC0000000	/* Rounding direction (bits 30-31) */
#define	FSR_RP		0x30000000	/* Rounding precision (bits 28-29) */
#define	FSR_TEM		0x0F800000	/* Trap Enable Mask   (bits 23-27) */
#define	FSR_AU		0x00400000	/* Abrupt Underflow   (bit  22)    */
					/* Reserved	      (bits 17-21) */
#define	FSR_FTT		0x0001C000	/* FP Trap Type       (bits 14-16) */
#define	FSR_QNE		0x00002000	/* Queue Not Empty    (bit  13)    */
					/* Reserved	      (bit  12)    */
#define	FSR_FCC		0x00000C00	/* FP Condition Codes (bits 11-10) */
#define	FSR_AEXC	0x000003E0	/* Accrued Exceptions (bits  9- 5) */
#define	FSR_CEXC	0x0000001F	/* Current Exceptions (bits  0- 4) */

/* FP Trap Types */
#    define	FSR_TT_NONE	0	/* Trap Type None */
#    define	FSR_TT_EXC	1	/* IEEE fp exception */
#    define	FSR_TT_UNFIN	2	/* Unfinished fp operation */
#    define	FSR_TT_UNIMP	3	/* Unimplemented fp operation */
#    define	FSR_TT_SEQERR	4	/* Sequence Error */

#endif	/*  __FSR_H__ */
