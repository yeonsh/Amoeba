/*	@(#)mmutil.S	1.5	96/02/27 13:59:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: mmutil.S -- manage mmu voodoo that *must* be done from assembly.
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 * Modified: Gregory J. Sharp, Hans van Staveren,
 *		until it sort of worked :-)
 */

#include "assyntax.h"
#include "mmuconst.h"
#include "machdep.h"
#include "fault.h"
#include "map.h"
#include "psr.h"
#include "offset.h"

#define SETCTX( ctx, oldctx, tmp1 )			\
	set	SSADR_CONTEXT,tmp1;			\
	lduba	[ tmp1 ] ASI_CONTROL, oldctx;		\
	stba	ctx, [ tmp1 ] ASI_CONTROL;		\
	WAIT_FOR_STATE_REG();

#define UNSETCTX( oldctx, tmp )				\
	set	SSADR_CONTEXT,tmp;			\
	stba	oldctx, [ tmp ] ASI_CONTROL;		\
	WAIT_FOR_STATE_REG();



/*
 * mmu_initcache() -- clear and initialize the memory cache, which merely
 * means zeroing the cache tags and turning on the bit in the system enable
 * register.
 */
GLOBNAME(mmu_initcache)

#ifdef USE_CACHE
	set	SSADR_CTAGS, %o0
	set	CACHE_SIZE, %o1
	set	CACHE_LINE_SIZE, %o2
mmui_loop:
	subcc	%o1, %o2, %o1
	bne	mmui_loop
	sta	%g0, [ %o0 + %o1 ] ASI_CONTROL	!!

	set	SSADR_ENA, %o0
	lduba	[ %o0 ] ASI_CONTROL, %o1
	or	%o1, ENA_CACHE, %o1
	stba	%o1, [ %o0 ] ASI_CONTROL
	WAIT_FOR_STATE_REG();

	mov	1, %o1
	STORE( %o1, %o0, GLNAME(mmu_cache) );
#endif	/* USE_CACHE */

	retl
	nop

/*
 * mmu_set_sysenb_reg(flags) - this routine ORs the flags specified
 * by the argument into the system enable register.
 */
GLOBNAME(mmu_set_sysenb_reg)
	set	SSADR_ENA, %o2
	lduba	[ %o2 ] ASI_CONTROL, %o1
	or	%o1, %o0, %o1
	stba	%o1, [ %o2 ] ASI_CONTROL
	WAIT_FOR_STATE_REG();
	retl
	nop

#ifdef DEBUG
GLOBNAME(mmu_get_sysenb_reg)
	set	SSADR_ENA, %o1
	lduba	[ %o1 ] ASI_CONTROL, %o0
	retl
	nop
#endif

/*
 * mmu_offcache() -- turn off the cache
 */
GLOBNAME(mmu_offcache)
	set	SSADR_ENA, %o0
	lduba	[ %o0 ] ASI_CONTROL, %o1
	andn	%o1, ENA_CACHE, %o1
	stba	%o1, [ %o0 ] ASI_CONTROL
	WAIT_FOR_STATE_REG();

#ifdef USE_CACHE
	STORE( %g0, %o0, GLNAME(mmu_cache));
#endif

	retl
	nop				!!

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
	set	MMU_SEVAR, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %o2
	mov	%o2, %i5

	/* Clear synchronous error register value */
	set	MMU_SER, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %o1

	/* Async error addr reg catchs sync error as well, so clear it */
	set	MMU_AEVAR, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %o4

	/* Clear async regs since they latch on sync errors as well */
	set	MMU_AER, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %o3

	set	mmua_msg, %o0

	call	GLNAME(printf)
	nop				!!

	restore
#else
	/* Synch error address register value as sixth arg */
	set	MMU_SEVAR, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %o5

	/* Clear synchronous error register value */
	set	MMU_SER, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %g0

	/* Async error addr reg catchs sync error as well, so clear it */
	set	MMU_AEVAR, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %g0

	/* Clear async regs since they latch on sync errors as well */
	set	MMU_AER, %R_TMP1
	lda	[%R_TMP1] ASI_CONTROL, %g0
#endif
	ba	GLNAME(mmu_pagefault)
	nop				!! Never to return again (to us)

#if PRINTF_LEVEL >= 2
	.align	8
mmua_msg:
	.ascii	"mmua: ser %x, sevar %x, aer %x, aevar %x\n\0"
	.align	8
#endif

/*
 * mmu_getpte( ctx, va ) -- return the pagenumber associated with a
 * given piece of virtual memory. The caller might want to mmu_probe() the
 * page first, to ensure that it gets backed by a PMEG.
 */
GLOBNAME(mmu_getpte)
	SETCTX( %o0, %g1, %g3 );

	lda	[ %o1 ] ASI_PAGE_MAP, %o0	/* Get it's PTE */

	UNSETCTX( %g1, %g3 );
	retl
	nop

/*
 * mmu_setpte( ctx, va, pte ) -- set the pte for the given virtual address
 * in the given context
 */
GLOBNAME(mmu_setpte)
	SETCTX( %o0, %g1, %g3 );

	sta	%o2, [ %o1 ] ASI_PAGE_MAP	/* Set it's PTE */

	UNSETCTX( %g1, %g3 );
	retl
	nop				!!

/*
 * mmu_getpmeg( ctx, va ) -- return the PMEG for with the given virtual
 * address in the given context
 */
GLOBNAME(mmu_getpmeg)
	SETCTX( %o0, %g1, %g3 );

	lduba	[ %o1 ] ASI_SEG_MAP, %o0	/* Fetch the PMEG */

	UNSETCTX( %g1, %g3 );
	retl
	nop

/*
 * mmu_setpmeg( ctx, va, pmeg ) -- set the PMEG for with the given virtual
 * address in the given context
 */
GLOBNAME(mmu_setpmeg)
	SETCTX( %o0, %g1, %g3 );

	stba	%o2, [ %o1 ] ASI_SEG_MAP	/* Set the PMEG */

	UNSETCTX( %g1, %g3 );
	retl
	nop				!!

/*
 *  And now some routines to flush the cache ...
 */

/*
 * mmu_flushcache() - mark all cache entries invalid
 */
GLOBNAME(mmu_flushcache)

#ifdef USE_CACHE
	set	SSADR_CTAGS, %o0
	set	CACHE_SIZE, %o1
	set	CACHE_LINE_SIZE, %o2
m_cache:
	subcc	%o1, %o2, %o1
	bne	m_cache
	sta	%g0, [ %o0 + %o1 ] ASI_CONTROL	!!
#endif	/* USE_CACHE */

	retl
	nop				!!

/*
 * mmu_flushseg( ctx, addr )  -- flush the given PMEG from the cache in the
 * specified context.
 */
GLOBNAME(mmu_flushseg)

#ifdef USE_CACHE
	LOAD( GLNAME(mmu_cache), %o4 );
	cmp	%o4, %g0
	be	mfs_cache_off
	nop				!!

	SETCTX( %o0, %g1, %g3 );
	set	CACHE_SIZE, %o2		! set the counter
	set	(MMU_SEGSIZE-1), %o0
	andn	%o1, %o0, %o1		! round address down to segment boundary
mfs_loop:
	subcc	%o2, CACHE_LINE_SIZE, %o2
	sta	%g0, [%o1] ASI_FLUSH_SEG
	bge	mfs_loop
	add	%o1, CACHE_LINE_SIZE, %o1	!!

	UNSETCTX( %g1, %g3 );
mfs_cache_off:

#endif /* USE_CACHE */
	retl
	nop				!!

/*
 * mmu_flushctx( ctx, psp ) -- flush the given hardware context completely
 * out of the MMU and reload with the given procseg mapping.
 */
GLOBNAME(mmu_flushctx)
	save	%sp, -MINFRAME, %sp
	SETCTX( %i0, %g1, %g3 );

#ifdef USE_CACHE
	LOAD( GLNAME(mmu_cache), %o0 );
	cmp	%o0, %g0
	be	mfc_nocache
	nop				!!
/*
 * Do the flush context of context in %i0.  We unroll the loop to gain
 * speed.  We do 16 cache lines per loop.
 */
	set	(CACHE_SIZE / 16), %l0

/* Start with last line in each of the 16 chunks.  We work backwards. */
	sub	%l0, CACHE_LINE_SIZE, %i0

	add	%l0, %l0, %l1
	add	%l1, %l0, %l2
	add	%l2, %l0, %l3
	add	%l3, %l0, %l4
	add	%l4, %l0, %l5
	add	%l5, %l0, %l6
	add	%l6, %l0, %l7

	add	%l7, %l0, %o0
	add	%o0, %l0, %o1
	add	%o1, %l0, %o2
	add	%o2, %l0, %o3
	add	%o3, %l0, %o4
	add	%o4, %l0, %o5
	add	%o5, %l0, %i4
	 
	sta	%g0, [%i0] ASI_FLUSH_CONTEXT
mfc_flushloop:
	sta	%g0, [%i0 + %l0] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %l1] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %l2] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %l3] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %l4] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %l5] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %l6] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %l7] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %o0] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %o1] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %o2] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %o3] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %o4] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %o5] ASI_FLUSH_CONTEXT
	sta	%g0, [%i0 + %i4] ASI_FLUSH_CONTEXT
	subcc	%i0, CACHE_LINE_SIZE, %i0	! decrement loop 
	bge,a	mfc_flushloop
	sta	%g0, [%i0] ASI_FLUSH_CONTEXT	!!


mfc_nocache:
#endif	/* USE_CACHE */

	/* Set all PMEGs for this context to fill-on-demand */
	set	USERLOW, %o0
	set	MMU_SEGSIZE, %o5
	set	USERHIGH - MMU_SEGSIZE, %o3

mfc_pmeg:
	ld	[ %i1 + PS_PMEG ], %o2
	stba	%o2, [ %o0 ] ASI_SEG_MAP
	add	%i1, SIZEOF_PROCSEG, %i1
	cmp	%o3, %o0
	bne	mfc_pmeg
	add	%o0, %o5, %o0			!! Move to next PMEG

	UNSETCTX( %g1, %g3 );

	ret
	restore			!!


/*
 * flush_cache( pid, addr, len ) -- Flush the cache selectively
 */
GLOBNAME(flush_cache)
	save	%sp, -MINFRAME, %sp
#ifdef USE_CACHE
	cmp	%i0, 0			! Zero implies kernel process
	be,a	fc_kernelpid
	mov	KERN_CTX, %i0		!! (proabaly zero anyway)

	sll	%i0, 2, %i0		! Make into a word
	LOAD( GLNAME(pidtoctx), %i5 );
	ld	[ %i5 + %i0 ], %i0
	cmp	%i0, -1
	be	fc_exit
	nop				!!

fc_kernelpid:
	SETCTX( %i0, %g1, %g3 );

	! %i2 = ( addr + len + PAGESIZE - 1 ) & ~( PAGESIZE - 1 );
	set	PAGESIZE-1, %i5
	add	%i1, %i2, %i2
	add	%i2, %i5, %i2
	andn	%i2, %i5, %i2

	andn	%i1, %i5, %i1		! addr &= ~( PAGESIZE - 1 )
	sub	%i2, %i1, %i2		! Actual length to flush

	mov	0x10, %l0;	mov	0x20, %l1;
	mov	0x30, %l2;	mov	0x40, %l3;
	mov	0x50, %l4;	mov	0x60, %l5;
	mov	0x70, %l6;	mov	0x80, %l7;
	mov	0x90, %o0;	mov	0xA0, %o1;
	mov	0xB0, %o2;	mov	0xC0, %o3;
	mov	0xD0, %o4;	mov	0xE0, %o5;
	mov	0xF0, %i5;

fc_loop:
	subcc	%i2, 16 * CACHE_LINE_SIZE, %i2
	sta	%g0, [ %i1 + %g0 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %l3 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %l7 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %o3 ] ASI_FLUSH_PAGE

	sta	%g0, [ %i1 + %l0 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %l4 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %o0 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %o4 ] ASI_FLUSH_PAGE

	sta	%g0, [ %i1 + %l1 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %l5 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %o1 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %o5 ] ASI_FLUSH_PAGE

	sta	%g0, [ %i1 + %l2 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %l6 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %o2 ] ASI_FLUSH_PAGE
	sta	%g0, [ %i1 + %i5 ] ASI_FLUSH_PAGE

	bg	fc_loop
	add	%i1, CACHE_LINE_SIZE * 16, %i1	!!

	UNSETCTX( %g1, %g3 );

fc_exit:
#endif	/* USE_CACHE */
	ret
	restore				!!


GLOBNAME(mmu_getctx)
	set	SSADR_CONTEXT, %o5		/* Get context register */
	retl
	lduba	[ %o5 ] ASI_CONTROL, %o0

GLOBNAME(mmu_setctx)
	set	SSADR_CONTEXT, %o5		/* Set context register */
	retl
	stba	%o0, [ %o5 ] ASI_CONTROL

/*
 * mmu_sync_err()
 *	Puts the info in the global variables synch_vaddr & synch_err.
 */
GLOBNAME(mmu_sync_err)
	/* Save synch error address register and friends for nmi trap routine */
	set	MMU_SEVAR, %o0
	lda	[ %o0 ] ASI_CONTROL, %o1
	STORE(%o1, %o2, GLNAME(synch_vaddr));

	set	MMU_SER, %o0
	lda	[ %o0 ] ASI_CONTROL, %o1
	STORE(%o1, %o2, GLNAME(synch_err));
	retl
	nop				!!

/*
 * mmu_nmi_ack() - must be a leaf routine!!!
 */
GLOBNAME(mmu_nmi_ack)
	/* Save synch error address register and friends for nmi trap routine */
	set	MMU_SEVAR, %o0
	lda	[ %o0 ] ASI_CONTROL, %o1
	STORE(%o1, %o2, GLNAME(synch_vaddr));

	set	MMU_SER, %o0
	lda	[ %o0 ] ASI_CONTROL, %o1
	STORE(%o1, %o2, GLNAME(synch_err));

	set	MMU_AEVAR, %o0
	lda	[ %o0 ] ASI_CONTROL, %o1
	STORE(%o1, %o2, GLNAME(asynch_vaddr));

	set	MMU_AER, %o0
	lda	[ %o0 ] ASI_CONTROL, %o1
	STORE(%o1, %o2, GLNAME(asynch_err));

	/*
	 * Toggle the enable-all bit in the interrupt register to clear
	 * the pending nmi interrupt.
	 */
	LOAD( GLNAME(interrupt_reg_p), %o0)	/* Get the pointer */
	ld	[ %o0 ], %o1			/* Get contents of register */
	andn	%o1, IER_ENALL, %o2
	st	%o2, [ %o0 ]
	retl
	st	%o1, [ %o0 ]		!!


#ifdef USE_CACHE
	GLOBBSS( GLNAME(mmu_cache), 4 );
#endif
