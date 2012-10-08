/*	@(#)mmutil.S	1.3	96/02/27 14:00:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "microsparc.h"
#include "sparc-refmmu.h"
#include "offset.h"
#include "fault.h"

/*
 * rd_mmu_reg(address) in ASI_MMU_REGS
 */

GLOBNAME(rd_mmu_reg)

	retl
	lda	[ %o0 ] ASI_MMU_REGS, %o0		!!

/*
 * wr_mmu_reg(address, value)
 */

GLOBNAME(wr_mmu_reg)

	retl
	sta	%o1, [ %o0 ] ASI_MMU_REGS		!!
/*
 * rd_tlb_entry(address) in ASI_MMU_DIAG
 */

GLOBNAME(rd_tlb_entry)

	retl
	lda	[ %o0 ] ASI_MMU_DIAG, %o0		!!

/*
 * rd_phys_addr(address) in ASI_MMU_BYPASS
 */

GLOBNAME(rd_phys_addr)

	retl
	lda	[ %o0 ] ASI_MMU_BYPASS, %o0		!!

/*
 * wr_phys_addr(address, value)
 */

GLOBNAME(wr_phys_addr)

	retl
	sta	%o1, [ %o0 ] ASI_MMU_BYPASS		!!

/*
 * mm_probe(virtaddr)
 */

GLOBNAME(mm_probe)

	set	0xFFF, %o1
	andn	%o0, %o1, %o0
	set	0x400, %o1
	or	%o0, %o1, %o0
	retl
	lda	[ %o0 ] ASI_MMU_FLPROBE, %o0		!!


/* CACHE FLUSHING */
/******************/
/*
 * For the microSPARC I
 *
 *	msI_data_cache_flash_clear(vir_bytes, vir_bytes)
 *	msI_instr_cache_flash_clear(void)
 *
 * The arguments to the first one are to shut lint up - we ignore the args.
 */

GLOBNAME(msI_data_cache_flash_clear)

	retl
	sta	%g0, [ %g0 ] ASI_DATA_CACHE_FLCLEAR	!!

GLOBNAME(msI_instr_cache_flash_clear)

	retl
	sta	%g0, [ %g0 ] ASI_INST_CACHE_FLCLEAR	!!

/*
 * For the microSPARC II
 *
 *	msII_do_cache_flush(addr, size, cache_line_size)
 *	msII_cache_flush_all(cache_size, cache_line_size)
 *
 * The first flushes cache entries in the range addr to addr+size-1
 * from both I and D caches in all contexts.  This routine can only
 * be called from the cache_flush routine in mm.c which provides us
 * with the cache line size and ensures that we have properly aligned
 * addresses!
 *
 * The second writes directly into the cache tags to flush the cache
 * lines.
 */

GLOBNAME(msII_do_cache_flush)

1:	subcc	%o1, %o2, %o1
	sta	%g0, [%o0] ASI_FLUSH_PAGE
	bcc	1b
	add	%o0, %o2, %o0				!!
	retl
	nop						!!

GLOBNAME(msII_do_cache_flush_all)

1:	subcc	%o0, %o1, %o0
	sta	%g0, [%o0] ASI_INST_CACHE_TAG
	bne	1b
	sta	%g0, [%o0] ASI_DATA_CACHE_TAG		!!
	retl
	nop
	

/*
 * mm_flush(va)
 */

GLOBNAME(mm_flush)

	retl
	sta	%g0, [%o0] ASI_MMU_FLPROBE
/*
 * mmu_accesstrap() -- called as the trap_function[] for both instruction
 * access exceptions and data access exceptions. We just read the error
 * from the bus error registers, clear the error, and then jump to the
 * 'C' routine 'mmu_pagefault()', inserting the error address as the fifth
 * argument.
 */
GLOBNAME(mmu_accesstrap)

	/* We want to see if we came from inside the mmu_probe routine. */
	set	GLNAME(sparc_prbeg), %R_TMP1
	cmp	%o2, %R_TMP1
	blu	mmua_notprobe
	nop				!!

	set	GLNAME(sparc_prend), %R_TMP1
	cmp	%o2, %R_TMP1
	bgeu	mmua_notprobe
	nop				!!

	/*
	 * Since we have not done a "save" yet, we can still touch the
	 * trap window's local registers. We use this fact to set up our
	 * return address to point to the instruction after the fault.
	 */
	add	%R_PC, 4, %R_PC		! Skip over trap insn
	add	%R_NPC, 4, %R_NPC

mmua_notprobe:

#if PRINTF_LEVEL >= 2
	save	%sp, -MINFRAME, %sp

	/* Synch error address register value as sixth arg */
	set	MMREG_SFAR, %R_TMP1
	lda	[%R_TMP1] ASI_MMU_REGS, %o2
	mov	%o2, %i5

	/* Clear synchronous error register value */
	set	MMREG_SFSR, %R_TMP1
	lda	[%R_TMP1] ASI_MMU_REGS, %o1

	set	mmua_msg, %o0

	call	GLNAME(printf)
	nop				!!

	restore
#else
	/* Synch error address register value as sixth arg */
	set	MMREG_SFAR, %R_TMP1
	lda	[%R_TMP1] ASI_MMU_REGS, %o5

	/* Clear synchronous error register value */
	set	MMREG_SFSR, %R_TMP1
	lda	[%R_TMP1] ASI_MMU_REGS, %g0
#endif
	ba	GLNAME(mmu_pagefault)
	nop				!! Never to return again (to us)

#if PRINTF_LEVEL >= 2
	.align	8
mmua_msg:
	.ascii	"mmua: sfsr %x, sfar %x\n\0"
	.align	8
#endif
