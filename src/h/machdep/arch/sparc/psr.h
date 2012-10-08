/*	@(#)psr.h	1.2	94/04/06 16:41:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: psr.h -- bits in the sparc PSR
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#ifndef _PSR_H_
#define _PSR_H_

#ifndef _ASM
struct psr {
	unsigned int
		psr_impl :4,	       	/* implementation number */
		psr_ver :4,	       	/* version number */
		psr_icc :4,		/* IU's condition codes */
		psr_reserved :6,      	/* reserved */
		psr_ec :1,		/* Coprocessor Enable*/
		psr_ef :1,		/* Floating-point Enable */
		psr_pil :4,		/* Processor Interrupt Level */
		psr_s :1,		/* Supervisor */
		psr_ps :1,		/* Previous supervisor */
		psr_et :1,		/* Trap enable */
		psr_cwp :5;		/* Current window pointer */
};
#endif	/* _ASM */

/* Bits for psr_icc */
#define ICC_NEG		0x08
#define ICC_ZERO	0x04
#define	ICC_OVERFLOW	0x02
#define ICC_CARRY	0x01

/* Simpler way of building these in locore */
#define ICC_SHIFT	20

#define PSR_IMPL	0xF0000000	/* SPARC implementation */
#define PSR_VER		0x0F000000	/* SPARC version (of impl) */
#define	PSR_ALLCC	0x00F00000	/* all cc bits */
#define	PSR_C		0x00100000	/* carry bit */
#define	PSR_V		0x00200000	/* overflow bit */
#define	PSR_Z		0x00400000	/* zero bit */
#define	PSR_N		0x00800000	/* negative bit */
#define PSR_RESVRD	0x000FC000	/* Reserved (must be zero) */
#define PSR_EC	    	0x00002000	/* Enable coprocessor */
#define PSR_EF	    	0x00001000	/* Enable floating point unit */
#define PSR_PIL	      	0x00000F00	/* Processor Interrupt Level */
#define PSR_S		0x00000080	/* Supervisor bit */
#define PSR_PS		0x00000040	/* Previous supervisor bit */
#define PSR_ET		0x00000020	/* Enable traps */
#define PSR_CWP		0x0000001F	/* Current window pointer */

#define PSR_PIL_SHIFT	8		/* Shift to get pil field */

#define	PSR_MBZ		PSR_RESVRD	/* must be zero bits */
#define	PSR_USERSET	(PSR_ET)
#define	PSR_USERCLR	(PSR_IPL|PSR_MBZ|PSR_PS)

/*
 * The first user PSR. It looks wrong, because the user should never be in
 * supervisor mode and should always have traps enable. This is in fact the
 * psr we set in supervisor mode just before doing a RETT, so interrupts must
 * be disabled and we must be in supervisor mode. Note that trap_end() will
 * take care of setting the CWP properly for us.
 */
#define USER_PSR	PSR_S

/*
 * This actually tells us if we *came* from user mode, not if we are
 * *in* user mode. The kernel uses it to test if a trap came from user mode.
 */
#define USERMODE( psr )	!( (psr) & PSR_PS )

#endif	/*  _PSR_H_ */
