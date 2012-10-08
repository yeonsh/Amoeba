/*	@(#)khead.S	1.4	96/02/27 13:59:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: khead.S -- deal with initial MMU mappings to get the kernel loaded
 * into the proper position with the proper page and segment mapping.
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 *	   - made Sun4c version.
 * Modified: Gregory J. Sharp, Hans van Staveren
 *	   - hacked it around until it worked for sun4m.
 */

#ifndef _ASM
#define _ASM
#endif

#include "map.h"
#include "assyntax.h"
#include "psr.h"
#include "fault.h"
#include "mmuconst.h"
#include "machdep.h"
#include "offset.h"
#include "microsparc.h"
#include "sparc-refmmu.h"

	.text
GLOBTAG(entry)
	mov	%o0, %l7		/* Save the ROM pointer */

/*
 * Map in 16 MB kernel memory in ctx 0 (KERNBASE maps to Physical address 0).
 */
	set	KERNBASE, %l1
	set	MMREG_CTPR, %l0
	lda	[ %l0 ] ASI_MMU_REGS, %l0	/* CTPR */
	sll	%l0, 4, %l0
	lda	[ %l0 ] ASI_MMU_BYPASS, %l0	/* PTE level 0 for CTX 0 */
	srl	%l0, 4, %l0
	sll	%l0, 8, %l0
	srl	%l1, 24-2, %l1			/* offset in PTE0 */
	set	PTE_ACC____RWX+PTE_ET_PTE, %l2	/* Page table entry */
	sta	%l2, [ %l0 + %l1 ] ASI_MMU_BYPASS
	set	0x400, %l0			/* Flush TLB */
	sta	%g0, [ %l0 ] ASI_MMU_FLPROBE

copy_down:
	/*
	 * Now we need to copy the kernel to it's new home.
	 * Pray that the current address and where it is going to be
	 * copied to are both mapped in in the current context.  If
	 * we came from the PROM this is definitely the case.
	 */
	set	KERNLOAD, %l0
call_insn:
	call	nearby			/* Need to get pc */
	nop				!!
nearby:
	sub	%o7, call_insn - entry, %l1
	set	_edata, %l6
	sub	%l6, %l0, %l2
	
copyloop:
	ld	[ %l1 ], %l6
	st	%l6, [ %l0 ]
	add	%l1, 4, %l1
	subcc	%l2, 4, %l2
	bnz	copyloop
	add	%l0, 4, %l0		!!

	set	startwork, %l2		! Need absolute symbol
	jmp	%l2			! Get to work ....
	nop				!!

startwork:
	/* Change to context 0 - in case we weren't in it */
	set	MMREG_CR, %l0
	sta	%g0, [ %l0 ] ASI_MMU_REGS
	nop
	nop
	nop

	call	GLNAME(clearbss),0		/* Zero begin to end */
	nop				!!

	STORE( %l7, %l0, GLNAME(prom_devp) );	/* Safe to do after clearbss */

	/*
	 * At some point in the future someone will run the reboot command.
	 * This calls bootkernel() which will need to put us back on the
	 * prom stack that we are no on before jumping into the new kernel
	 */
	STORE( %sp, %l0, prom_stack );

	/*
	 * Set the stack pointer to the initial kernel stack.
	 * Terminate it so we don't trace back beyond it.
	 */
	set	GLNAME(kst_end), %l0
	sub	%l0, MINFRAME, %sp
	mov	-1, %fp

	/*
	 * Fill in the machine structure.
	 * The following must be done early since some mmu parameters
	 * are lurking in the machine structure, along with architecture
	 * specific things.  It is however machine specific since it depends
	 * on the type of sun you have ...
	 *
	 * But first ... count the # register windows
	 */
	mov	%wim, %g1	! save wim
	sub	%g0, 1, %g2	! put -1 in g2
	mov	%g2, %wim
	WAIT_FOR_STATE_REG();
	mov	%wim, %g2	! This should now be a bitmask containing
				! the valid windows.
	mov	%g1, %wim	! restore wim
	WAIT_FOR_STATE_REG();
	
	mov	%g2, %o0	! give find_machine the win mask
	call	GLNAME(find_machine), 1
	nop				!!

	call	GLNAME(main), 0
	nop				!!

	call	GLNAME(abort), 0
	nop				!!

GLOBNAME(restore_prom_stack)
	set	prom_stack, %sp
	retl
	nop				!!

/*
 * The first block below keeps the bss from starting in the same page the
 * text or data segment ends in. The second one is a sparc specific bit of
 * cleverness (I hope):
 *
 * We want to take as little care as possible when flushing user windows out
 * to the stack, but (to be secure) we must ensure that the user has not
 * given us a dangerous stack pointer, a flush of which we damage or currupt
 * our kernel. But this is too big a bother.
 *
 * So we cheat: we reserve context zero for the kernel and allow *no* user
 * process there, though the kernel process may use it. Then, rather than
 * map the entire kernel into each context, we map only up to (and including)
 * `end_of_machine', including the structure 'machine' into each of the other
 * hardware contexts. Then we are flush all windows out to the user stack
 * without worrying about the possiblity of hitting valuable kernel data.
 *
 * In any case, the second block below works to pad the machine structure so
 * that the page that it is in can be marked read-only.
 */

	.seg	"data"

#ifndef ROMKERNEL
	! to prevent data and bss in same page
#ifdef notdef
paddingfordata:
	.skip	PAGESIZE
#endif
#endif
	.global	GLNAME(begin)
GLNAME(begin):	.word	4

/* For restoring the prom stack on a bootkernel call */
	.global prom_stack
prom_stack:	.word	4
