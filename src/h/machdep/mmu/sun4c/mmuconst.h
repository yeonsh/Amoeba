/*	@(#)mmuconst.h	1.3	94/04/06 16:48:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: mmuconst.h -- constants associated with the sun4c mmu
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#ifndef __MMUCONST_H
#define __MMUCONST_H

#include "machdep.h"
#include "map.h"

#define PAGESHIFT	12
#define PAGESIZE	4096

/* Convert from bytes to pages */
#define BTOPG( nb )	( (nb) >> PAGESHIFT )
#define PGTOB( npg )	( (npg) << PAGESHIFT )

#define RNDUP( x, y )   (( (x) + ((y)-1)) & ~((y)-1))
#define RNDDN( x, y )   ( (x) & ~((y)-1))

#define MAXALIGN        8               /* Largest required alignment */

/*
 * These can be over-ridden by each kernel's "map.h"
 */
#ifndef MMU_MAXPMEG
#define MMU_MAXPMEG	0x100		/* Max for any sun4c MMU */
#endif
#ifndef	NUMSEGS
#define NUMSEGS		(8*nproc)	/* Number of software segments */
#endif

#define PROM_CTX	0		/* Where the prom lives initially */
#define DVMA_CTX	0
#define KERN_CTX	PROM_CTX	/* The main kernel context */

/*
 * Bits in the sun4c MMU's PTE
 */
#define	PTE_V		0x80000000	/* Valid bit */
#define	PTE_PROT	0x60000000	/* Protection bits */
#define PTE_WR		0x40000000	/* Write-able */
#define	PTE_KN		0x20000000	/* Kernel mode only */
#define PTE_NOCACHE	0x10000000	/* Don't cache bit */
#define PTE_SPACE(n)	(((n)&3)<<26)
#define PTE_IO		PTE_SPACE(1)	/* IO Space (0=mem,1=sbus) */
#define PTE_A		0x02000000	/* Accessed bit */
#define	PTE_M		0x01000000	/* Modified bit */
#define	PTE_PFNUM	0x000fffff	/* Physical page number */ 

#define	PTE_KW		(PTE_WR|PTE_KN)
#define	PTE_UW		PTE_WR
#define	PTE_URKR	0
#define	PTE_KR		PTE_KN

#define PTE_USRREAD	(PTE_V)		/* User read-only */
#define PTE_USRWRITE	(PTE_V|PTE_WR)	/* User writable */
#define PTE_KERNREAD	(PTE_V|PTE_KN)	/* Kernel read-only */
#define PTE_KERNWRITE	(PTE_V|PTE_KN|PTE_WR)	/* Kernel writable */

#define PTE_SIZE	sizeof( int )
#define PTE_SHIFT	2		/* log2( PTE_SIZE ) */

/* Convert from page counts to pte counts */
#define PGTOPTEB( npg )		( (npg) << PTE_SHIFT )
#define PTEBTOPG( npte )	( (npte) >> PTE_SHIFT )

#ifndef _ASM

/*
 * Create the PTE for a given _valid_ kernel virtual address, by fetching
 * it from 'sysmap'.
 */
extern int *sysmap;
#define MMU_MAKEPTE( addr )		\
    sysmap[ BTOPG( VTOP((int) (addr)) - KERNBASE ) ]

#endif	/* _ASM */

/*
 * While hardware segment size seems to be fairly stable, it is MMU dependant
 * and so is defined here. If other SPARC machines start using other hardware
 * segment sizes, you might go ahead and throw this in mach_info.
 */
#define MMU_SEGSHIFT	18		/* log2( hardware segment size ) */
#define MMU_SEGSIZE	(1<<MMU_SEGSHIFT)

/*
 * These define the addresses of the memory error registers as they sit in
 * ASI_CONTROL space.
 */
#define MMU_SER		(SSADR_BUSERR+0)  /* Sync Error Register */
#define MMU_SEVAR	(SSADR_BUSERR+4)  /* Sync Error Virtual Addr Reg */
#define MMU_AER		(SSADR_BUSERR+8)  /* Async Error Register */
#define MMU_AEVAR	(SSADR_BUSERR+12) /* Async Error Virtual Addr Reg */

#define PMEG_KERNEL	0xFFFFFFFF	/* Kernel owned PMEG */
#define PMEG_FRESH	0		/* Newly allocated PMEG */
#define PMEG_MAXAGED	0xFFFFFFFE	/* Take care not to wrap to zero */

#define MMU_IOPMEG		0x6F	/* Map IO register through here */
#define MMU_KERNPMEG		0x70	/* First PMEG used by kernel */
#define MMU_KERNPMEG_LAST	0x75	/* First PMEG not used by kernel */
#define MMU_PMEGMAP1		0x75	/* Two pmegs to use for emergency */
#define MMU_PMEGMAP2		0x76
#define MMU_FODPMEG		0x77	/* Fill-on-demand PMEG */
#define MMU_PROMPMEG		0x78	/* First PMEG used bu PROM */
#define MMU_INVPMEG		0x7F	/* The invalid PMEG */

/*
 * The sun4c's cache is arranged as a block of CACHE_SIZE, with CACHE_LINE_SIZE
 * bytes per line. It operates on virtual memory addresses, mapping into the
 * cache in a simple modulo CACHE_SIZE. So checking if a given virtual address
 * in cached is just:
 *	([ SSADR_CTAGS | (va & CACHE_MASK) ] ASI_CONTROL ) & CT_VALID
 */
#define CACHE_SIZE		(64*1024)
#define	CACHE_LINE_SHIFT	4
#define CACHE_LINE_SIZE		(1<<CACHE_LINE_SHIFT)
#define CACHE_MASK		0xFFF0	/* Mask out index into ctags space */

/*
 * The VALID bit is the only one we *might* care about, and I'll bet I
 * end up just flushing without even checking it, just for speed.
 */
#define CT_VALID	bit( 19 )	/* Cache entry is valid */

#define LOG2_SIZEOF_PROCSEG	3	/* No nice way to generate this */

/* The "3" here is for log2( sizeof( struct procmap )) */
#define	PIDTOPMAP_SHIFT 	(USERHIGH_SHIFT - MMU_SEGSHIFT + 3)

#ifndef _ASM

#define	NUM_USERSEGS	( USERHIGH >> MMU_SEGSHIFT )
#define SEGNUM( addr )	( (addr) >> MMU_SEGSHIFT )
#define SEGOFF( addr )	(( (addr) & ( MMU_SEGSIZE - 1 )) >> CLICKSHIFT )

#define NPTEperSEG	(MMU_SEGSIZE/PAGESIZE)	/* Number of PTEs per segment */

/*
 * These structures describes the address to segment mappings for one
 * process. The pm_pmeg field is either -1 or a pmeg that holds the
 * mappings for this segment.
 */
struct procseg {
    int	*ps_pte;
    int	ps_pmeg;
};

struct procmap {
    struct procseg pm_seg[ NUM_USERSEGS ];
};

/*
 * We need to do PMEG aging.  This is done by keeping a linked list of
 * least recently stolen PMEGs.  PMEGs allocated permanently are not in
 * this list.  Freed pmegs are added to the head.  Newly allocated PMEGs
 * are moved to the tail.  A PMEG is free if owned by PID -1.
 */
struct pmegmap {
    int		pm_pid;			/* Process ID number (or -1) */
    vir_bytes	pm_address;		/* Address of PMEG in context */
    struct pmegmap *pm_next;		/* For LRS list */
};

#endif	/* _ASM */

#define	__bit(n)	(1 << (n))

/* Memory Error Register Bits */
/******************************/

#define	MER_PARITY		__bit(7)
#define	MER_MULTIPLE		__bit(6)
#define	MER_PARITY_TEST		__bit(5)
#define	MER_PARITY_ENABLE	__bit(4)
#define	MER_BYTE3		__bit(3)	/* Parity error was in byte 3 */
#define	MER_BYTE2		__bit(2)	/* Parity error was in byte 2 */
#define	MER_BYTE1		__bit(1)	/* Parity error was in byte 1 */
#define	MER_BYTE0		__bit(0)	/* Parity error was in byte 0 */

/* Synchronous Error Register Bits */
/***********************************/

#define	SER_WRITE		__bit(15)
#define	SER_INVALID		__bit(7)
#define	SER_PROT_ERR		__bit(6)
#define	SER_TIMEOUT		__bit(5)
#define	SER_SBUS_ERR		__bit(4)
#define	SER_MEM_ERR		__bit(3)
#define	SER_SIZE_ERR		__bit(1)
#define	SER_WATCHDOG		__bit(0)

/* Asynchronous Error Register Bits */
/************************************/

#define	ASER_WBACKERR		__bit(7)
#define	ASER_TIMEOUT		__bit(5)
#define	ASER_DVMAERR		__bit(4)

#endif	/* __MMUCONST_H */
