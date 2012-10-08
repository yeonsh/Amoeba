/*	@(#)map.h	1.4	96/02/16 15:50:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: map.h -- configuration constants
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 *         Greg Sharp, Sept 1991 - Added more device addresses that the PROM
 *				   admitted knowing about.  Got rid of non-
 *				   standard debug printf.
 */

#ifndef _MAP_H
#define _MAP_H

#if 1
#define USE_CACHE
#endif

#define BIT( x )	( 1 << (x))

#define LOGNTRANS		4    	/* Define here even if defaults */
#define LOGNRECV		5

#define	TICKS_PER_SECOND	100

#define BUSADDR(x)      ethermap( (phys_bytes) (x))

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

/*
 * Initial attempt at a memory map:
 *
 * Memory map: Virtual:
 *	       0	PROM wastage
 *	   1b000	End Prom wastage
 *	   20000	User Memory
 *	20000000	Top of user memory (start of address hole)
 *	E0000000	Printbuf Buffer
 *	E0002999	End of Printbuf
 *	E0003000	Trap Table (TBR)
 *	E0004000	Kernel Start
 *	FC000000	End of kernel memory
 *	FF000000	Lance DVMA area (beginning with init area)
 *    	FF800000	End Lance DVMA area
 *	FFD00000	Start of PROM memory
 *	FFF00000	(supposed) End of PROM memory
 *
 *	The area between E0000000 and FC000000 is of interest in that there are
 *	no real pagetables here. Memory references cause page faults which
 *	cause the mmu_pagefault() routine to fill in the appropriate ptes,
 *	based on it's knowledge of how physical memory is mapped into virtual
 *	kernel space.
 *
 *	The area between 20000000 and E0000000 is a hole in the virtual memory
 *	address space, since the MMU demands that the top two bits (that is,
 *	bits 31 and 30) of any virtual address be copies of the next bit
 *	(that is, bit 29). Any address reference into this hole will fault.
 *
 * Memory map: Physical:
 *	       0	PROM work/load area (we land somewhere in here)
 *	   1b000	End Prom Wastage
 *	   20000	First address passed to initmem (thru end of chunk #1)
 *	 2000000	Kernel memory at virt E0000000
 *
 * Nothing else in physical memory is magical, but, in order to
 * get loaded portably, I want to not touch anything in the first
 * chunk, nor to depend on the size of the chunks. So all I do is to reverse
 * the order in which the first two chunks are mapped into virtual memory.
 *
 * The value 1b000 is chosen after examination of the PROM. According to SUN,
 * the PROM wants to eat all addresses from FFD00000 to FFF00000, and all pages
 * mapped into this range. The first `1b' pages are mapped into this range, so
 * we swear never to touch them. The PROM also asks (ha! asks) us not to touch
 * the PMEGs associated with these pages, which include:
 *	0, 0x37, 0x78-0x7F
 * So again we swear. And then we curse. And then we get on with life.
 * (When I become sure that these areas are only used for the prom IO device,
 * I'll release any claim to them and remove this comment.)
 *
 * The PROM seems even more unforgiving of late; it wants to hold on to the
 * lowest segment of address spaces, even if we are not using the prom IO
 * device. So I'm adjusting USERLOW (and VIR_LOW in machdep.h) to compensate.
 */

/* See also VIR_LOW and VIR_HIGH from machdep.h */
#define USERLOW_SHIFT		MMU_SEGSHIFT
#define USERHIGH_SHIFT		29
#define USERLOW			(1<<USERLOW_SHIFT)
#define USERHIGH		(1<<USERHIGH_SHIFT)

#define KERNBASE		(0xE0000000)
#define IVECBASE		KERNBASE
#define PB_START		((struct printbuf *) (KERNBASE+0x1000))
#define PB_SIZE			0x3000		/* Make a larger print buffer */
#define KERNLOAD		(0x00004000+KERNBASE) /* == PB_START+PB_SIZE */
#define KERNEND			0xFC000000
#define	SCSIMEM			0xFD000000
#define LANCEMEM		0xFF000000
#define IOBASE			0xFF800000
#define PROMLOW			0xFFD00000
#define PROMHIGH		0xFFF00000

#define KERNELSTART	((vir_bytes) KERNBASE)	/* kernel starts here */

/*
 * Magical addresses for the sun4c EEPROM, IDPROM, and other PROM memory
 */
#ifndef V2MMU
#define PROM_ADDRESS	0xFFE80010		/* Use if hardware forgot */
#define EEPROM_ADDRESS	0xF2000000		/* EEPROM in OBIO space */
#define IDPROM_ADDRESS	0xF20007D8		/* IDPROM in OBIO space */

/* These are mapped by the prom into it's address space; all are OBIO */
#define DVADR_MOUSE		0xFFD00000	/* PA F0000000 */
#define DVADR_KBD		0xFFD00004	/* PA F0000004 */
#define DVADR_SERIALB		0xFFD02000     	/* PA F1000000 */
#define DVADR_SERIALA		0xFFD02004	/* PA F1000004 */
#define DVADR_EEPROM		0xFFD04000	/* PA F2000000 */
#define DVADR_IDPROM		0xFFD047D8	/* PA F20007D8 */
#define PROM_IDADDR		0xFFD047D9	/* ID byte as mapped by PROM */
#define DVADR_CLOCK		0xFFD047F8	/* PA F20007F8 */
#define DVADR_COUNTER		0xFFD06000	/* PA F3000000 */
#define	DVADR_ERROR_REGS	0xFFD16000	/* PA F4000000 */
#define DVADR_INTERRUPT_REG	0xFFD0A000	/* PA F5000000 */
#define	DVADR_AUX_IO_REG	0xFFD0E000	/* PA F7400000 */
#define	DVADR_DMA		0xFFD14000	/* PA F8400000 */
#define	DVADR_SCSI		0XFFD12000	/* PA F8800000 */
#define	DVADR_VIDEO_DAC		0xFFD1C000	/* PA FA400000 */
#define DVADR_FRAME_BUF		0xFFD80000	/* PA FA800000 */

/* We map this one: the constant is the physical address */
#define DVADR_LANCE		0xF8C00000	/* -> Virtual LANCEMEM */
#endif

/*
 * Alternate address Space Indicators
 */
#define ASI_CONTROL            2       /* also called system space */
#define ASI_SEG_MAP            3
#define ASI_PAGE_MAP           4
#define ASI_USER_PROGRAM       8
#define ASI_KERN_PROGRAM       9
#define ASI_USER_DATA          10
#define ASI_KERN_DATA          11
#define ASI_FLUSH_SEG          12
#define ASI_FLUSH_PAGE         13
#define ASI_FLUSH_CONTEXT      14

/*
 * Addresses of interesting items in system space
 */
#define SSADR_CONTEXT	0x30000000	/* Context register */
#define SSADR_ENA	0x40000000	/* System enable register */
#define SSADR_BUSERR	0x60000000	/* Bus error registers */
#define SSADR_CTAGS	0x80000000	/* Cache tags */
#define SSADR_CDATA	0x90000000	/* Cache data */
#define SSADR_SPORT	0xF0000000	/* Serial port w/ MMU bypass */

/*
 * The system enable register (not to be confused with the synchronous error
 * register; ahh the joy on the letter game).
 */
#define ENA_RESET	BIT( 2 )	/* Reset the CPU chip */
#define ENA_CACHE	BIT( 4 )       	/* Enable caching */
#define ENA_SDVMA	BIT( 5 )	/* All DVMA is enabled */
#define	ENA_NOTBOOT	BIT( 7 )	/* Set once MMU is initialised */

/*
 * Bits in the sun4c's interrupt enable register. Bits for software interrupts
 * request those interrupts, bits for hardware interrupts enable just those
 * interrupts, and BIT(0) enables all interrupts.
 */
#define IER_ENALL	BIT( 0 )
#define IER_SOFT1	BIT( 1 )
#define IER_SOFT4	BIT( 2 )
#define IER_SOFT6	BIT( 3 )
#define IER_HARD10	BIT( 5 )
#define IER_HARD14	BIT( 7 )

#ifndef _ASM

extern uint16 nproc, nthread;		/* From conf.c */

#endif	/* _ASM */
#endif	/* _MAP_H */
