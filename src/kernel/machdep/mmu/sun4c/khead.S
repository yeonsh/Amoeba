/*	@(#)khead.S	1.4	96/02/27 13:59:33 */
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

	.text
GLOBTAG(entry)
	mov	%o0, %l7		/* Save the ROM pointer */

/*
 * First we want to set up some address mapping so that we can map the kernel
 * into the proper spot and jump there before calling main. We start with
 * PMEG# 0x70 and map thru 0x74, leaving these as static PMEGs for all
 * time (giving us room for a 1.25 meg kernel). I guess we should come around
 * in initmm() and steal back and PMEGs that map anything after _end.
 *
 * Then we map the kernel to this new area and jump to it. So simple even my
 * dog can do it. Off we go then....
 */

	/* Map kernel segments into kernel memory */
	set	KERNBASE, %l0		/* Where we want to start mapping */
	mov	MMU_KERNPMEG, %l1	/* First PMEG to map */
	mov	MMU_KERNPMEG_LAST-MMU_KERNPMEG, %l2	/* # of PMEGS */
	set	MMU_SEGSIZE, %l3	/* Size of area mapped by each PMEG */

segloop:
	stba	%l1, [ %l0 ] ASI_SEG_MAP
	add	%l0, %l3, %l0
	subcc	%l2, 1, %l2
	bnz	segloop
	add	%l1, 1, %l1		!!

	set	4*1024, %l3
	set	12, %l4
	set	KERNBASE, %l0

	clr	%l1			/* Physical pagenumber */
	mov	MMU_KERNPMEG_LAST - MMU_KERNPMEG, %l2
	sll	%l2, MMU_SEGSHIFT, %l2

	set	PTE_V|PTE_KN|PTE_WR, %l5
	or	%l5, %l1, %l1		/* Fold in the PTE flags */

	/*
	 * In case you were sleeping, the registers are:
	 *	l0	virtual address we are mapping
	 *	l1	physical page number and pte flags
	 *	l2	number of bytes to map
	 *	l3	bytes per page
	 *	l4	pageshift
	 *	l5	PTE flags
	 */
pageloop:
	sta	%l1, [ %l0 ] ASI_PAGE_MAP
	add	%l0, %l3, %l0		! Next virtual page
	subcc	%l2, %l3, %l2		! Are we done yet?
	bnz	pageloop
	add	%l1, 1, %l1		!! Next physical page

	/* Now we just need to copy the kernel to it's new home */
	set	KERNLOAD, %l0
call_insn:
	call	nearby			/* Need to get pc */
	nop				!!
nearby:
	sub	%o7, call_insn - entry, %l1
	set	_edata, %l6
	set	KERNLOAD, %l2
	sub	%l6, %l2, %l2
	
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
	call	GLNAME(clearbss),0	/* Zero begin to end */
	nop				!!

	STORE( %l7, %l0, GLNAME(prom_devp) ); /* Safe to do this after clearbss */

	/*
	 * Set the stack pointer to the initial kernel stack.
	 */
	set	GLNAME(kst_end), %l0
	sub	%l0, MINFRAME, %sp

	/*
	 * Fill in the machine structure.
	 * The following must be done early since some mmu parameters
	 * are lurking in the machine structure, along with architecture
	 * specific things.  It is however machine specific since it depends
	 * on the type of sun4c you have ...
	 */
	call	GLNAME(find_machine), 0
	nop				!!

	call	GLNAME(main), 0
	nop				!!

	/*
	 * main() has returned or someone has called abort(); in either case
	 * we want to reset the kernel. For now, we'll just exit to the ROM
	 * monitor, but eventually we need to get prom_devp and prom_id into
	 * registers, mark their page writable, and jump to the label
	 * 'startwork'.
	 */
GLOBNAME(abort)
	mov	KERN_CTX, %l0
	set	SSADR_CONTEXT, %l1
	stba	%l0, [ %l1 ] ASI_CONTROL
	WAIT_FOR_STATE_REG();

	LOAD( GLNAME(prom_devp), %l0 );
	ld	[ %l0 +PD_EXIT ], %l0
	jmp	%l0
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
paddingfordata:
	.skip	PAGESIZE
#endif
	/* The symbol begin should really be at the start of the bss seg,
	 * but the Sun and Gnu loader don't seem to be willing to do
	 * this for us..
	 */
	/* .seg	"bss" */
	.global	GLNAME(begin)
GLNAME(begin):	.word	4
