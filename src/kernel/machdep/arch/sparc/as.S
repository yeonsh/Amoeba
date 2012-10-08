/*	@(#)as.S	1.6	96/02/27 13:46:33 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: as.S -- handle the bottom half of interrupts and such. All
 * traps funnel in through trap_begin and out through trap_end with all
 * manner of things happening between the two. Overflow and underflow handlers
 * have their own entry points, for they do not have a full window of
 * registers.
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 * Modified: Hans van Staveren & Greg Sharp, November 1992
 *	     Hacked around in the trap code so that user processes die
 *	     on illegal memory references and stack overflow, rather than
 *	     the kernel crashing :-)  Also got the fp support working.
 *	     Kees Verstoep, July 1994: optimised window overflow handling
 *	     Leendert van Doorn & Greg Sharp, got rid of most of the window
 *	     overflow/underflow security problems.
 */

#include "assyntax.h"
#include "machdep.h"
#include "mmuconst.h"
#include "map.h"
#include "psr.h"
#include "fsr.h"
#include "fault.h"
#include "offset.h"
#include "syscall_stub.h"

#ifdef ASI_SEG_MAP
#define SUN4c
#endif

#ifndef SUN4c
/* Currently !SUN4c implies SUN4m.  Define SUN4M_OPTIMISE to get a faster
 * (sun4m specific) window overflow/underflow trap handling.
 */
#define SUN4M_OPTIMISE
#endif

#define SC_nop	5

	.text
	.global		GLNAME(trap_table)
GLNAME(trap_table)	=	IVECBASE

#define	WSIZE		(15 * 4)	/* Last word in a register window */

#define ARGVA	%g1	/* Input: Virtual address to map or unmap */
#define VA	%g1	/* Internally used the same way */
#define ARGPSR	%g2	/* Input: original PSR (at start of trap) */
#define PTE1	%g2	/* The PTE for the start of the window */
#define	PTE2	%g3	/* The PTE for the end of the window (VA+WSIZE) */
#define ENDP	%g3	/* The address of the end of the window (VA+WSIZE) */
#define BASE	%g4	/* Generic base pointer (for page tables) */
#define	OFF	%g5	/* Generic offset (for page tables) */
#define TMP	%g6	/* Temporary */
#define RC	%g6	/* Return code (to caller) */
#define RETADDR %g7	/* Return address (well, retaddr+8) */

/*
 * Return codes for trap_map(). The values of these return code are chosen
 * with care; they are also offsets into the trap_mappings[] if STATISTICS
 * are defined.
 * TM_OKAY implies that the mapping went fine; TM_NADA means that no mapping
 * was required (and no unmapping is needed); TM_BAD means something went
 * wrong, and the memory has not been mapped.
 */
#define TM_OKAY		0		/* Good return code */
#define TM_NADA		4		/* Very good: we did no work */
#define TM_BAD		8		/* Bad return code */

#ifdef SUN4c
/*
 * Check to see if the ptes for address 'va' and 'endp' are backed
 * by valid PTEs. This is done by fetching the PTEs from ASI_PAGE_MAP
 * space, and bitwise-anding them together with the PTE_V bit. The
 * resultant condition codes can be tested by a "bnz" to 'valid' code.
 */
#define CHECK_VALPTES( va, endp, pte1, pte2, tmp )	\
	lda	[ va ] ASI_PAGE_MAP, pte1;		\
	lda	[ endp ] ASI_PAGE_MAP, pte2;		\
	and	pte1, pte2, tmp;			\
	set	PTE_V, pte1;				\
	andcc	pte1, tmp, %g0;

/*
 * trap_map() -- map two pages (and segments) into real memory, making sure
 * that they are valid kernel or user pages.
 *
 * The overflow and underflow handlers have the tricky job of dealing with
 * the possibility that the memory where the window should be flushed out
 * to, for overflow, or fetched in from, for underflow, might not be backed
 * with a PMEG, although amoeba guarantees us that the page within the PMEG
 * will not be stolen. So we look at both the segment containing the page
 * that the stack pointer (%sp) points to, and the next segment (I'm being
 * lazy here, since it is easier to check the next segment than to figure
 * out if the end of the stack frame lies in the next segment, and then have
 * to check it anyways). If either segment is MMU_FODPMEG, we replace it
 * with our special mapping PMEGs (MMU_PMEGMAP[12]) and fill in the PTE
 * from either the kernel's memory map (sysmap) or the current process's
 * memory map (curpmap), checking to see that we over step the bounds of
 * neither array.
 *
 * The PMEG mapping might need some more thought later, as it seems that the
 * user's stacks will never see anything except overflows and underflows,
 * and may never get a more permanent mapping. This could be fixed by calling
 * a higher level routine to get real PMEG, either after doing all the work
 * (in the case of an overflow, which must make the window valid before calling
 * anyone), or before doing the work (in the case of the underflow, which has
 * all the windows at its command). Or perhaps just doing this sort of thing
 * at underflow time, when it is clearly less expensive, will suffice.
 *
 * In any case arguments to this routine come as:
 *	%g1	virtual address to map
 *	%g2	original PSR
 *	%g6	return code (non-zero means error)
 * 	%g7	calling instruction's address
 * Register usage is described below; note that the caller *must* save all
 * globals somewhere safe before calling us, and that between the calls to
 * trap_map() and trap_unmap() the caller must not touch %g1-%g3, for we
 * depend on state kept there.
 *
 * While I do not like using these 'defines' for register names, it does
 * force more organized code construction and (hopefully) readability. Plus
 * it showed me where I was wasting one register. ;^)
 * Be particularly cafeful that some of these symbolic names point to the
 * same register. In some cases this is obvious (ARGVA,VA) in some it is
 * of no consequence (TMP,RC) and in some it is definitely dangerous
 * (PTE2,ENDP), but I thought it was still helpful. Be careful. Don't Panic.
 */

trap_map:
	andcc	ARGPSR, PSR_PS, %g0	! USERMODE( psr )
	bz	tmap_usermode		! Go fetch from user map
	nop				!!

	set	KERNBASE, TMP		! Check if the kernel is touching ...
	cmp	TMP, VA			! ... a user address
	bgu	tmap_usermode		! join user mode code below
	nop				!!

	/*
	 * We want to check very quickly to see if this mapping is needed
	 * at all; we trust the kernel to touch valid addresses only, so
	 * we need only check to see if the addresses VA thru VA+WSIZE are
	 * backed by valid PTEs (and therefore PMEGs)
	 */
	add	VA, WSIZE, ENDP
	CHECK_VALPTES( VA, ENDP, PTE1, PTE2, TMP );
	bnz	tmap_exit		! if valid, we can skip the excitment
	mov	TM_NADA, RC		!! ... with a 'did-nothing' return

	/*
	 * More complicated, we must find the proper kernel PTEs
	 */
	LOAD( GLNAME(sysmap), BASE )	! System (kernel) page table
	cmp	BASE, 0			! Normal if sysmap is NULL
	be	tmap_nullsysmap		! Go make the best of things
	sub	VA, TMP, OFF		!! Make virtual, relative to KERNBASE

	set	PAGESHIFT, TMP

	add	OFF, WSIZE, ENDP	! Find end of window
	srl	ENDP, TMP, ENDP		! ... and make into a byte offset
	sll	ENDP, 2, ENDP		! ... into the page table

	srl	OFF, TMP, OFF		! OFF is now virtual page number
	sll	OFF, 2, OFF		! OFF is now offset into page table

#ifndef NDEBUG
	LOAD( GLNAME(syssize), TMP )	! Number of pages in sysmap
	sll	TMP, 2, TMP		! Make TMP into a byte count also

	cmp	ENDP, TMP		! Are we off the end of the page table
	bgeu	tmap_bad		! ... Then we have nothing we can do;
	nop				!!
#endif	/* NDEBUG */

	ld	[ BASE + OFF ], PTE1	! Fetch PTE of first page
	ba	tmap_eithermode
	ld	[ BASE + ENDP ], PTE2	!! Fetch PTE of second page

tmap_nullsysmap:
	/*
	 * Sysmap is null, which means that we are very early in the kernel,
	 * so we just let the map succeed without watching it. This
	 * should never occur, as the sysmap must be filled before
	 * FOD PMEGs can be used, but we check anyway.
	 */
	ba	tmap_exit		! Pretend all is fair
	mov	TM_NADA, RC		!! ... with a 'did-nothing' return

tmap_usermode:
	/*
	 * We want to check very quickly to see if this mapping is needed
	 * at all; if the addresses are strictly less than KERNBASE, then
	 * we know that (if valid) they must be user memory. So we
	 * need only check to see if the addresses VA thru VA+WSIZE are
	 * back by valid PTEs (and PMEGs)
	 */
	andcc	VA, 7, TMP		! If stack pointer is not aligned
	bnz	tmap_bad		! we ax the process.
	nop				!!

	add	VA, WSIZE, ENDP
	set	KERNBASE, TMP
	cmp	ENDP, TMP		! if ( va+WSIZE >= KERNBASE )	
	bgeu	tmap_bad		! ... then we break
	nop				!!

	CHECK_VALPTES( VA, ENDP, PTE1, PTE2, TMP );
	bnz	tmap_exit		! If valid, we can skip the excitment
	mov	TM_NADA, RC		!! ... with a 'did-nothing' return

	/* First we find the PTE for the start of the window */
	srl	VA, MMU_SEGSHIFT, OFF	! Find segment number
	LOAD( GLNAME(curpmap), BASE );	! Get current mappings
	sll	OFF, LOG2_SIZEOF_PROCSEG, OFF
	ld	[ BASE + OFF ], BASE	! Now we have the pte for this segment

	cmp	BASE, %g0		! If the segment is zero
	be	tmap_bad		! ...then the reference is bad
	nop				!!

	set	PAGESHIFT, TMP
	set	MMU_SEGSIZE - 1, OFF	! Find segment offset
	and	VA, OFF, OFF		! Get segment offset bits
	srl	OFF, TMP, OFF		! Turn into a page number
	sll	OFF, 2, OFF		! Turn into PTE offset
	ld	[ BASE + OFF ], PTE1	! This is the lower-bound PTE

	/* Now we find the PTE for the upper end of the window */
	add	VA, WSIZE, OFF
	srl	OFF, MMU_SEGSHIFT, OFF	! Find segment number
	LOAD( GLNAME(curpmap), BASE );	! Get current mappings
	sll	OFF, LOG2_SIZEOF_PROCSEG, OFF
	ld	[ BASE + OFF ], BASE	! Now we have the pte for this segment

	set	MMU_SEGSIZE - 1, OFF	! Find segment offset
	and	VA, OFF, OFF		! Get segment offset bits
	srl	OFF, TMP, OFF		! Turn into a page number
	sll	OFF, 2, OFF		! Turn into PTE offset
	ld	[ BASE + OFF ], PTE2	! This is the upper-bound PTE

	set	PTE_V, TMP		! For the laugh, check if the PTEs
	and	TMP, PTE1, TMP		! are valid.
	andcc	TMP, PTE2, TMP
	bz	tmap_bad
	nop				!!

tmap_eithermode:

	add	VA, WSIZE, OFF		!!
	mov	MMU_PMEGMAP1, TMP	! Set up for mapping by hand
	stba	TMP, [ VA ] ASI_SEG_MAP ! Set PMEG to emergency map1
	mov	MMU_PMEGMAP2, TMP
	stba	TMP, [ OFF ] ASI_SEG_MAP ! Set PMEG to emergency map2
	sta	PTE1, [ VA ] ASI_PAGE_MAP
	sta	PTE2, [ OFF ] ASI_PAGE_MAP

tmap_okay:
	mov	TM_OKAY, RC		! No error
tmap_exit:
#ifdef STATISTICS
	set	GLNAME(trap_mapping), %g5 ! Increment stat variables
	ld	[ %g5 + RC ], %g4
	add	%g4, 1, %g4
	st	%g4, [ %g5 + RC ]
#endif
	jmp	RETADDR+8		! Return to caller
	nop				!!

tmap_bad:
	ba	tmap_exit
	mov	TM_BAD, RC		!! Serious error

/*
 * Now the caller does whatever it wants with the memory we just (perhaps)
 * mapped, and then it must call trap_unmap() to remove any lasting damage.
 * trap_unmap() returns nothing interesting.
 */
trap_unmap:
	/* Restore the FOD PMEGs, for we did not map entire segment */
	mov	MMU_FODPMEG, TMP	! Put the FOD PMEG back
	add	VA, WSIZE, ENDP
	stba	TMP, [ VA ] ASI_SEG_MAP
	stba	TMP, [ ENDP ] ASI_SEG_MAP

	jmp	RETADDR+8		! Return to caller
	nop				!!

#else /* SUN4c */

/* Macro for inlined trap_map() calls.  The code immediately following the
 * SUN4M_TRAP_MAP call is supposed to contain an error handler.
 */
#define SUN4M_TRAP_MAP(addr, oklabel)	\
	btst	PSR_PS, ARGPSR;		\
	bnz	1f;			\
	nop;				\
	set	KERNBASE-WSIZE, TMP;	\
	cmp	addr, TMP;		\
	bgeu	2f;			\
	nop;				\
1:	btst	0x7, addr;		\
	bnz	2f;			\
	nop;				\
	andn	addr, 0xFFF, TMP;	\
	or	TMP, 0x400, TMP;	\
	lda	[ TMP ] 3, TMP;		\
	tst	TMP;			\
	bne	oklabel;		\
	nop;				\
2:

	.seg	"text"
trap_map:
	/*
	 * We want to check very quickly to see the sp is "safe"(TM).
	 * If the addresses are strictly less than KERNBASE, then we know
	 * that (if valid) they must be user memory. So we need only check
	 * to see if the addresses VA thru VA+WSIZE are valid.
	 */
	btst	PSR_PS, ARGPSR		! USERMODE( psr )
	bnz	1f
	nop				!!

	set	KERNBASE-WSIZE, TMP
	cmp	VA, TMP			! if ( va+WSIZE >= KERNBASE )	
	bgeu	2f			! ... then we break
	nop				!!
1:
	btst	0x7, VA			!! If stack pointer is not aligned
	bnz	2f			!  we ax the process.
	nop				!!

	andn	VA, 0xFFF, TMP
	or	TMP, 0x400, TMP
	lda	[ TMP ] 3, TMP		! ASI_MMU_FLPROBE
	tst	TMP
	bne	3f
	mov     TM_NADA, RC             !! No error
2:
	mov	TM_BAD, RC		! else the probe failed
3:
	jmp	RETADDR+8		! Return to caller
	nop				!!
#endif /* SUN4c */

/*
 * trap_begin() -- We just got a trap and have bounced into kernel mode, from
 * either kernel or user mode. If we came from user mode, we must flush the
 * entire register file to the stack (see note above for why we do this) and
 * then must switch to context zero (the kernel context). Then we can get the
 * current pid and stash it in TU_PID
 */

GLOBTAG(trap_begin)
	mov	%tbr, %R_TBR		! Save some more
	mov	%y, %R_Y		! Save even more (whadda sale, bah!)

	/*
	 * We need to be able to use the global registers, so we find the
	 * address on a fictitious stack frame and save them into it. The
	 * stack frame actually will be created farther down in the code,
	 * so we are in fact writing *below* the current bottom of the stack,
	 * but this will not bother us, as interrupts are disabled and must
	 * remain disabled until we set the stack pointer farther down.
	 */
	andcc	%R_PSR, PSR_PS, %g0	! If we came from kernel mode
	bnz	tb_kernstack		! ... then we do not need to bother
	sub	%fp, SIZEOF_THREAD_USTATE - TU_GLOBAL, %R_TMP1	!!

	LOAD( GLNAME(kern_stack), %R_TMP1 );	! Fetch kernel stack pointer
	add	%R_TMP1, TU_GLOBAL, %R_TMP1
tb_kernstack:
	SAVE_GLOBALS_TO_BUFFER( %R_TMP1 );

	INVALID_TEST( %R_TMP1, %R_TMP2 );
	bz	tb_overokay
	nop				!!

tbov_start:
	mov	%R_PSR, ARGPSR		! Save psr into global

	/* Get to window that we must overflow */
	save

#ifdef SUN4c
	set	trap_map, TMP		! Call trap_map( %sp )
	jmpl	TMP, RETADDR
	mov	%sp, ARGVA

	cmp	RC, TM_BAD
	bne	tbov_tm_ok
	nop				!!
#else
	/* optimised out trap_map call */
	SUN4M_TRAP_MAP(%sp, tbov_tm_ok)
#endif

#ifndef NOPROC
	LOAD	(GLNAME(bad_ustack), TMP) ! trap_map failed so flag it for later
	add	TMP, 1, TMP
	STORE	(TMP, %g7, GLNAME(bad_ustack))
#endif /* NOPROC */
	b	tbov_nopte
	nop				!!

tbov_tm_ok:
	/* Save the contents of the window to the stack */
	SAVE_WINDOW_TO_BUFFER( %sp );

#ifdef SUN4c
	cmp	RC, TM_NADA		! We did not map it
	be	tbov_nopte		! ... so we do not unmap it
	nop				!!

	set	trap_unmap, TMP		! Call trap_unmap()
	jmpl	TMP, RETADDR
	nop				!!
#endif

tbov_nopte:
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	ADVANCE_WIM( %g4, %g5, %g7 );
	restore

tb_overokay:

	andcc	%R_PSR, PSR_PS, %g0	! If we came from kernel mode
	bnz	kernel_trap		! ... then we do not need to bother
	nop				!! ... flushing the register file

	LOAD( GLNAME(kern_stack), %R_TMP1 );	! Fetch kernel stack pointer
	mov	%R_TMP1, %sp			! Set the stack pointer

	/*
	 * We want to flush the user's windows out to the stack, but we want
	 * to ensure that there has been no sabotage of stack pointers which
	 * would allow the user to write to kernel memory (since we are in
	 * supervisor mode), so we must check pointers carefully for each
	 * window that we flush. We cannot allow any traps, for the next
	 * trap would look like it came from kernel mode, and would flush
	 * registers without regard for the mischieveous user. We basically
	 * do a mangled version of "trap_overflow" (and now refer you to
	 * comments associated with that routine). However we flush these
	 * windows out in a loop, and must avoid overflow or underflow
	 * traps. So we must "restore" to each window between the previous
	 * one and the invalid one. We then reset the CWP field of the PSR
	 * to its value when we arrived in this trap handler. Note that for
	 * the calls to trap_{,un}map(), we must save "our" globals to the
	 * static savearea.
	 */

	ba	tb_bottomflush
	mov	%R_PSR, ARGPSR		!! Save psr into global

tb_topflush:
#ifdef SUN4c
	/*
	 * We need to be able to use the global registers, so I have
	 * a static savearea that I can always safely save them into.
	 * Since we only need ARGPSR, we save and restore it.
	 */
	set	global_savearea, TMP
	st	ARGPSR, [ TMP ]
#else
	/* ARGPSR (%g2) is not clobbered by trap_map, so no need to save
	 * and restore it on every iteration.
	 */
#endif

	/* Get to window that we must overflow */
	restore

#ifdef SUN4c
	set	trap_map, TMP		! Call trap_map( %sp )
	jmpl	TMP, RETADDR
	mov	%sp, ARGVA

	cmp	RC, TM_BAD
	bne	tb_tm_ok
	nop				!!
#else
	/* optimised out trap_map call */
	SUN4M_TRAP_MAP(%sp, tb_tm_ok)
#endif

#ifndef NOPROC
	LOAD	(GLNAME(bad_ustack), TMP)	! trap_map failed so flag it for later
	add	TMP, 1, TMP
	STORE	(TMP, %g7, GLNAME(bad_ustack))
#endif /* NOPROC */
	b	tb_nopte
	nop				!!

tb_tm_ok:
	/* Save the contents of the window to the stack */
	SAVE_WINDOW_TO_BUFFER( %sp );

#ifdef SUN4c
	cmp	RC, TM_NADA		! We did not map it
	be	tb_nopte		! ... so we do not unmap it
	nop				!!

	set	trap_unmap, TMP		! Call trap_unmap()
	jmpl	TMP, RETADDR
	nop				!!
#endif

tb_nopte:
#ifdef SUN4c
	set	global_savearea, TMP
	ld	[ TMP ], ARGPSR
#endif

tb_bottomflush:
	/*
	 * Check if we are at the edge of the world, looking off into the
	 * vastness of the invalid register.
	 */
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	UNDERFLOW_TEST( %g4, %g5, %g7 );
	bz	tb_topflush
	nop				!!

	mov	%psr, %g5
	andn	%g5, PSR_CWP, %g5
	and	ARGPSR, PSR_CWP, %g7
	or	%g5, %g7, %g5
	mov	%g5, %psr
	! WAIT_FOR_STATE_REG();

	/* Now we must set the %wim correctly */
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g6 );
	add	%g7, 1, %g7		! %g7 still holds the cwp

	cmp	%g6, %g7
	be,a	tb_cwpmax
	mov	%g0, %g7		!! Start at bottom
tb_cwpmax:
	mov	1, %g5
	sll	%g5, %g7, %g5
	mov	%g5, %wim
	! WAIT_FOR_STATE_REG();

#ifdef SUN4c
	/* Throw the context into the PID register for a minute */
	set	SSADR_CONTEXT, %R_TMP1
	lduba	[ %R_TMP1 ] ASI_CONTROL, %R_PID

	/* Get to the kernel context */
	set	SSADR_CONTEXT, %R_TMP2	/* Set hardware context register */
#if KERN_CTX == 0
	stba	%g0, [ %R_TMP2 ] ASI_CONTROL
#else
	mov	KERN_CTX, %R_TMP1
	stba	%R_TMP1, [ %R_TMP2 ] ASI_CONTROL
#endif
	WAIT_FOR_STATE_REG();

	/*
	 * Replace the hardware context number we currently have in R_PID
	 * with real pid from ctxtopid[].
	 */
	LOAD( GLNAME(ctxtopid), %R_TMP1 );
	sll	%R_PID, 2, %R_TMP2	! Make CTX number into word pointer
	ld	[ %R_TMP1 + %R_TMP2 ], %R_PID
#endif /* SUN4c */

	/* Save floating point registers */
	set	PSR_EF, %R_TMP2
	andcc	%R_PSR, %R_TMP2, %g0
	bz	either_trap
	nop				!!

	st	%fsr, [ %sp + TU_FSR ]
	std	%f0, [ %sp + TU_FLOAT + 4*0 ]
	std	%f2, [ %sp + TU_FLOAT + 4*2 ]
	std	%f4, [ %sp + TU_FLOAT + 4*4 ]
	std	%f6, [ %sp + TU_FLOAT + 4*6 ]
	std	%f8, [ %sp + TU_FLOAT + 4*8 ]
	std	%f10, [ %sp + TU_FLOAT + 4*10 ]
	std	%f12, [ %sp + TU_FLOAT + 4*12 ]
	std	%f14, [ %sp + TU_FLOAT + 4*14 ]
	std	%f16, [ %sp + TU_FLOAT + 4*16 ]
	std	%f18, [ %sp + TU_FLOAT + 4*18 ]
	std	%f20, [ %sp + TU_FLOAT + 4*20 ]
	std	%f22, [ %sp + TU_FLOAT + 4*22 ]
	std	%f24, [ %sp + TU_FLOAT + 4*24 ]
	std	%f26, [ %sp + TU_FLOAT + 4*26 ]
	std	%f28, [ %sp + TU_FLOAT + 4*28 ]
	std	%f30, [ %sp + TU_FLOAT + 4*30 ]

	/* Save floating-point queue */
	ld	[ %sp + TU_FSR ], %R_TMP1
	set	FSR_QNE, %g6
	andcc	%R_TMP1, %g6, %g0
	bz	tb_fq_empty
	mov	TU_FQUEUE, %R_TMP2		!! offset
tb_fp_loop:
	std	%fq, [ %sp + %R_TMP2 ]
	st	%fsr, [ %sp + TU_FSR ]
	ld	[ %sp + TU_FSR ], %R_TMP1
	andcc	%R_TMP1, %g6, %g0		! is queue now empty
	bnz	tb_fp_loop
	add	%R_TMP2, 8, %R_TMP2		!! offset++

tb_fq_empty:
	sub	%R_TMP2, TU_FQUEUE, %R_TMP2
	srl	%R_TMP2, 3, %R_TMP2		! convert offset to count
	st	%R_TMP2, [ %sp + TU_QSIZE ]	! store in queue size

	/* Disable fp unit */
	mov	%psr, %R_TMP1
	set	PSR_EF, %R_TMP2
	andn	%R_TMP1, %R_TMP2, %R_TMP1
	mov	%R_TMP1, %psr
	! WAIT_FOR_STATE_REG();

tb_nofloat:
	ba	either_trap
	nop				!!

kernel_trap:
	/* Make a stack frame */
	sub	%fp, SIZEOF_THREAD_USTATE, %sp
	LOAD( GLNAME(curpid), %R_PID );

either_trap:
	! If we have an NMI trap we had better acknowledge it before
	! we enable traps or it will keep coming back
	and	%R_TBR, TBR_MASK, %R_TMP1
	cmp	%R_TMP1, 0x1f0
	bne	not_nmi
	nop				!!
	call	GLNAME(mmu_nmi_ack)	! Must clear the nmi and save state
	nop				!! mmu_nmi_ack must be a leaf routine!

not_nmi:
	PSR_HIGHPIL( %R_TMP1 );		! Enable traps, but not interrupts

trap_door:
#ifndef NOPROC
	LOAD	(GLNAME(bad_ustack), TMP)
	tst	TMP
	be	trap_door_goodstack
	nop				!!
	STORE	(%g0, TMP, GLNAME(bad_ustack))
	call	GLNAME(trp_stkoverflow)
	mov	%sp, %o0		!!
	! never to return
#endif	/* NOPROC */

trap_door_goodstack:
	mov	%sp, %o1		! arg1 = stack pointer
	mov	%R_PC, %o2		! arg2 = pc
	mov	%R_PSR, %o3		! arg3 = psr
	mov	%R_PID, %o4		! arg4 = pid
	ld	[ %sp + TU_GLOBAL + 4 ], %o5	! arg5 = original g1

	/* Change the TBR registers into a word-based index */
	and	%R_TBR, TBR_MASK, %R_TMP1
	srl	%R_TMP1, TBR_SHIFT, %o0	! arg0 = tt field from tbr
	sll	%o0, 2, %R_TMP1		! Convert to word pointer

#ifdef	STATISTICS
	set	GLNAME(trap_count), %o7	! Need to count interrupt types
	ld	[ %o7 + %R_TMP1], %R_TMP2
	add	%R_TMP2, 1, %R_TMP2
	st	%R_TMP2, [ %o7 + %R_TMP1]
#endif	/* STATISTICS */

	set	GLNAME(trap_function), %R_TMP2
	ld	[ %R_TMP1 + %R_TMP2 ], %R_TMP1
	call	%R_TMP1, 5		! call the actual trap handler
	st	%R_PSR, [ %sp + TU_LOCAL + 0 ]	!!

		! get PSR back in case it changed
		! the trap routines are allowed to fiddle with the
		! memory copy.
	ld	[ %sp + TU_LOCAL + 0 ], %R_PSR

GLOBTAG(trap_end)

	andcc	%R_PSR, PSR_PS, %g0	! If we came from kernel mode
	bnz	te_kernelmode		! .. then we have a context
	nop				!!

	/*
	 * If the global variable schedlevel is set (by the upper layers),
	 * we must call the scheduler() function.
	 */
	LOADHALF( GLNAME(schedlevel), %R_TMP1 );
	cmp	%R_TMP1, %g0
	be	te_nosched
	nop				!!

	call	GLNAME(setints)		!  enable interrupts
	mov	0, %o0			!! setints(0)

	call	GLNAME(scheduler), 0
	nop				!!

te_nosched:

#ifndef	NOPROC

	/*
	 * The global variable 'thread_sigpend' is set when the current thread
	 * has a signal that needs to be handled. We call swtrap() to deal with
	 * such cases.
	 */

	LOAD( GLNAME(thread_sigpend), %R_TMP1 );
	cmp	%R_TMP1, 0		! non-zero ==> needs swtrap()
	be	te_noswtrap
	nop				!!

	call	GLNAME(setints)		!  enable interrupts
	mov	0, %o0			!! setints(0)

	/* Load arguments to swtrap() */
	LOAD( GLNAME(thread_sig), %o0 );       ! arg0 = thread_sig
	call	GLNAME(swtrap), 2
	mov	%sp, %o1		!! arg1 = stack pointer

	mov	%o0, %sp		! returns new sp

te_noswtrap:
#endif	/* NOPROC */

#ifdef SUN4c
	/*
	 * Refetch the context for this pid from the pidtoctx[] map. If our
	 * hardware context has been stolen (-1), we call mmu_allocctx() to
	 * get a valid context.
	 */
	LOAD( GLNAME(pidtoctx), %R_TMP1 );
	sll	%R_PID, 2, %R_TMP2	! Make CTX number into word pointer
	ld	[ %R_TMP1 + %R_TMP2 ], %R_TMP1
	cmp	%R_TMP1, -1
	bne	te_validctx
	nop				!!

	call	GLNAME(mmu_allocctx),1		! Go find a valid context
	mov	%R_PID, %o0		!!

	mov	%o0, %R_TMP1		! Move context into proper reg

te_validctx:
	/* Set curpid and curpmap */
	STORE( %R_PID, %R_TMP2, GLNAME(curpid) );
	sll	%R_PID, PIDTOPMAP_SHIFT, %R_PID
	LOAD( GLNAME(pmap), %R_TMP2 );
	add	%R_TMP2, %R_PID, %R_PID
	STORE( %R_PID, %R_TMP2, GLNAME(curpmap) );

	set	SSADR_CONTEXT, %R_TMP2	/* Set hardware context register */
	stba	%R_TMP1, [ %R_TMP2 ] ASI_CONTROL
	WAIT_FOR_STATE_REG();
#endif /* SUN4c */

	/* Restore floating point registers */
	set	PSR_EF, %R_TMP2
	andcc	%R_PSR, %R_TMP2, %g0
	bz	te_nofloat
	nop				!!

	mov	%psr, %R_TMP1
	or	%R_TMP1, %R_TMP2, %R_TMP1
	mov	%R_TMP1, %psr
	WAIT_FOR_STATE_REG();

	ld	[ %sp + TU_FSR ], %fsr
	WAIT_FOR_STATE_REG();
	ldd	[ %sp + TU_FLOAT + 4*0 ], %f0
	ldd	[ %sp + TU_FLOAT + 4*2 ], %f2
	ldd	[ %sp + TU_FLOAT + 4*4 ], %f4
	ldd	[ %sp + TU_FLOAT + 4*6 ], %f6
	ldd	[ %sp + TU_FLOAT + 4*8 ], %f8
	ldd	[ %sp + TU_FLOAT + 4*10 ], %f10
	ldd	[ %sp + TU_FLOAT + 4*12 ], %f12
	ldd	[ %sp + TU_FLOAT + 4*14 ], %f14
	ldd	[ %sp + TU_FLOAT + 4*16 ], %f16
	ldd	[ %sp + TU_FLOAT + 4*18 ], %f18
	ldd	[ %sp + TU_FLOAT + 4*20 ], %f20
	ldd	[ %sp + TU_FLOAT + 4*22 ], %f22
	ldd	[ %sp + TU_FLOAT + 4*24 ], %f24
	ldd	[ %sp + TU_FLOAT + 4*26 ], %f26
	ldd	[ %sp + TU_FLOAT + 4*28 ], %f28
	ldd	[ %sp + TU_FLOAT + 4*30 ], %f30

te_nofloat:

te_kernelmode:
	PSR_HIGHPIL( %R_TMP1 );		/* Disable interrupts */

	/* Deal with underflow() */
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	mov	%g7, %g6
	UNDERFLOW_TEST( %g4, %g5, %g6 );
	bz	te_underokay
	nop				!!

	mov	%R_PSR, ARGPSR		! Set PSR argument to trap_map()

	ld	[ %fp ], %g0		! Touch the stack to force a fault.

	/* Get to window that we must underflow */
	RETREAT_WIM( %g4, %g5, %g7 );
	restore				! This is the window to restore

#ifdef SUN4c
	set	trap_map, TMP		! Call trap_map( %sp )
	jmpl	TMP, RETADDR
	mov	%sp, ARGVA		!!

	cmp	RC, TM_BAD
	be	te_nopte
	nop				!!
#else
	/* optimised out trap_map call */
	SUN4M_TRAP_MAP(%sp, te_tm_ok)
	b	te_nopte
	nop				!!
#endif

te_tm_ok:
	/* Restore the contents of the window from the stack */
	RESTORE_WINDOW_FROM_BUFFER( %sp );

#ifdef SUN4c
	cmp	RC, TM_NADA		! We did not map it
	be	te_nopte		! ... so we do not unmap it
	nop				!!

	set	trap_unmap, TMP		! Call trap_unmap()
	jmpl	TMP, RETADDR
	nop				!!
#endif

te_nopte:
	save				! Get back to trap window

te_underokay:
	add	%sp, TU_GLOBAL, %R_TMP1
	RESTORE_GLOBALS_FROM_BUFFER( %R_TMP1 );

	andcc	%R_PSR, PSR_PS, %g0	! If we came from kernel mode
	bnz	te_kernstack		! ... then we do not need to bother
	nop				!!

	STORE( %sp, %R_TMP1, GLNAME(kern_stack) );	! Save kernel stack pointer
te_kernstack:
	/* Restore PSR and the minor registers */
	andn	%R_PSR, PSR_CWP, %R_TMP1
	mov	%psr, %R_TMP2
	and	%R_TMP2, PSR_CWP, %R_TMP2
	or	%R_TMP1, %R_TMP2, %R_TMP1
	mov	%R_TMP1, %psr
	WAIT_FOR_STATE_REG()

	mov	%R_Y, %y

	jmp	%R_PC
	rett	%R_NPC		!!

trap_die:
	ba	trap_die
	nop			!!

/*
 * trap_template() -- used as a template that gets copied into the trap
 * vector table for each interrupt that we want to claim as ours (with the
 * obvious exception of the underflow and overflow handlers).
 */
GLOBNAME(trap_template)
	set	trap_begin, %R_TMP1	! Make absolute address (two word insn
	jmp	%R_TMP1			! Jump to it
	mov	%psr, %R_PSR		! Save save save

#ifndef SUN4M_OPTIMISE
/*
 * trap_overflow() (INTERRUPT) -- handle an overflow trap by flushing the
 * lowest window onto the stack.
 */
GLOBTAG(trap_overflow)
	mov	%tbr, %R_TBR		! Save some more
	/*
	 * We need to be able to use the global registers, so I have
	 * a static savearea that I can always safely save them into.
	 */
	set	global_savearea, %l3
	SAVE_GLOBALS_TO_BUFFER( %l3 );
	mov	%R_PSR, ARGPSR		! Save psr into global

#ifdef STATISTICS
	set	GLNAME(trap_count) + ( TRAPV_WND_OVER * 4 ), %R_TMP1
	ld	[ %R_TMP1 ], %R_TMP2
	add	%R_TMP2, 1, %R_TMP2
	st	%R_TMP2, [ %R_TMP1 ]
#endif	/* STATISTICS */

	/* Get to window that we must overflow */
	save

	set	trap_map, TMP		! Call trap_map( %sp )
	jmpl	TMP, RETADDR
	mov	%sp, ARGVA

#ifndef NOPROC
	cmp	RC, TM_BAD
	be	to_illmemref
	nop				!!
#endif

	/* Save the contents of the window to the stack */
	SAVE_WINDOW_TO_BUFFER( %sp );

	cmp	RC, TM_NADA		! We did not map it
	be	over_nopte		! ... so we do not unmap it
	nop				!!

	set	trap_unmap, TMP		! Call trap_unmap()
	jmpl	TMP, RETADDR
	nop				!!

over_nopte:
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	ADVANCE_WIM( %g4, %g5, %g7 );
	restore

	set	global_savearea, %l3	! PARANOID: Should be set still
	RESTORE_GLOBALS_FROM_BUFFER( %l3 );

	mov	%R_PSR, %psr		! Restore PSR
	WAIT_FOR_STATE_REG();

	jmp	%R_PC
	rett	%R_NPC			!!

#else /* SUN4M_OPTIMISE */
/*
 * trap_overflow() (INTERRUPT) -- handle an overflow trap by flushing the
 * lowest window onto the stack.
 */
GLOBTAG(trap_overflow)
#ifdef STATISTICS
	/* First update statistics via R_TMP1 and R_TMP2, since R_TMP1 (l6)
	 * is used to save and restore g2.
	 */
	set	GLNAME(trap_count) + ( TRAPV_WND_OVER * 4 ), %R_TMP1
	ld	[ %R_TMP1 ], %R_TMP2
	add	%R_TMP2, 1, %R_TMP2
	st	%R_TMP2, [ %R_TMP1 ]
#endif	/* STATISTICS */

	/* Only save the globals we really use below into locals that
	 * we don't use.
	 */
	mov	ARGPSR, %l6
	mov	%g5, %l3	/* since R_Y is not used */
	mov	%g6, %l4	/* since R_TBR is not used */
	mov	%g7, %l5	/* since R_PID is not used */

	mov	%R_PSR, ARGPSR		! Save psr into global

	/* Get to window that we must overflow */
	save

	/* inlined trap_map() call */
	SUN4M_TRAP_MAP(%sp, to_tm_ok)
#ifndef NOPROC
	/* trap_map failed */
	b	to_illmemref
	nop				!!
#endif

to_tm_ok:
	/* Save the contents of the window to the stack */
	SAVE_WINDOW_TO_BUFFER( %sp );

	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	ADVANCE_WIM( %g6, %g5, %g7 );
	restore

	mov	%R_PSR, %psr		! Restore PSR
	/* WAIT_FOR_STATE_REG(); */
	/* Instead we can restore the globals while PSR is stabilizing: */
	mov	%l6, ARGPSR
	mov	%l3, %g5
	mov	%l4, %g6
	mov	%l5, %g7

	jmp	%R_PC
	rett	%R_NPC			!!
#endif /* SUN4M_OPTIMISE */

#ifndef NOPROC
	/* We had an illegal memory reference - we trash the register windows
	 * of the user process and change to kernel context.  Then we can
	 * call C code to kill off the offending process.
	 */
to_illmemref:
	LOAD( GLNAME(kern_stack), %R_TMP1 );	! Fetch kernel stack pointer
	mov	%R_TMP1, %sp		! Set the stack pointer
#ifdef SUN4c
	/* Get to the kernel context */
	set	SSADR_CONTEXT, %R_TMP2	! Set hardware context register
#if KERN_CTX == 0
	stba	%g0, [ %R_TMP2 ] ASI_CONTROL
#else
	mov	KERN_CTX, %R_TMP1
	stba	%R_TMP1, [ %R_TMP2 ] ASI_CONTROL
#endif /* KERN_CTX == 0 */
	WAIT_FOR_STATE_REG();
#endif /* SUN4c */
	PSR_HIGHPIL( %R_TMP1 );		! Enable traps, but not interrupts
	call	GLNAME(trp_stkoverflow)	! Now kill the little blighter
	mov	%sp, %o0		!! pass a frame pointer
	/* Should never get back here */
	unimp	0
#endif	/* NOPROC */

/*
 * trap_overflow_template() -- used as a template that gets copied into the
 * trap vector table for the overflow interrupt handler.
 */
GLOBNAME(trap_overflow_template)
	set	trap_overflow, %R_TMP1	! Make absolute address (two word insn
	jmp	%R_TMP1			! Jump to it
	mov	%psr, %R_PSR		! Save save save

#ifndef SUN4M_OPTIMISE
/*
 * trap_underflow() (INTERRUPT) -- handle an underflow trap by loaded the
 * lowest window from the stack.
 */
GLOBTAG(trap_underflow)
	mov	%tbr, %R_TBR			! Save some more

	/*
	 * We need to be able to use the global registers, so I have
	 * a static savearea that I can always safely save them into.
	 */
	set	global_savearea, %l3
	SAVE_GLOBALS_TO_BUFFER( %l3 );
	mov	%R_PSR, ARGPSR		! Save psr into global register

#ifdef	STATISTICS
	set	GLNAME(trap_count) + ( TRAPV_WND_UNDER * 4 ), %R_TMP1
	ld	[ %R_TMP1 ], %R_TMP2
	add	%R_TMP2, 1, %R_TMP2
	st	%R_TMP2, [ %R_TMP1 ]
#endif	/* STATISTICS */

	/* Get to window that we must underflow */
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	RETREAT_WIM( %g4, %g5, %g7 );
	restore				! This is window we trapped from
	restore				! This is the window to restore

	set	trap_map, TMP		! Call trap_map( %sp )
	jmpl	TMP, RETADDR
	mov	%sp, ARGVA

	cmp	RC, TM_BAD
	be	under_nopte
	nop				!!

	/* Restore the contents of the window from the stack */
	RESTORE_WINDOW_FROM_BUFFER( %sp );

	cmp	RC, TM_NADA		! We did not map it
	be	under_nopte		! ... so we do not unmap it
	nop				!!

	set	trap_unmap, TMP		! Call trap_unmap()
	jmpl	TMP, RETADDR
	nop				!!

under_nopte:
	save				! Get back to faulting window
	save				! Get back to trap window

	set	global_savearea, %l3	! PARANOID: Should be set still
	RESTORE_GLOBALS_FROM_BUFFER( %l3 );

	mov	%R_PSR, %psr		! Restore PSR
	WAIT_FOR_STATE_REG();

	jmp	%R_PC
	rett	%R_NPC			!!
#else /* SUN4M_OPTIMISE */
/*
 * trap_underflow() (INTERRUPT) -- handle an underflow trap by loaded the
 * lowest window from the stack.
 */
GLOBTAG(trap_underflow)
#ifdef	STATISTICS
	/* First update statistics via R_TMP1 and R_TMP2, since R_TMP1 (l6)
	 * is used to save and restore g2.
	 */
	set	GLNAME(trap_count) + ( TRAPV_WND_UNDER * 4 ), %R_TMP1
	ld	[ %R_TMP1 ], %R_TMP2
	add	%R_TMP2, 1, %R_TMP2
	st	%R_TMP2, [ %R_TMP1 ]
#endif	/* STATISTICS */

	/* Only save the globals we really use below into locals that
	 * we don't use.
	 */
	mov	ARGPSR, %l6
	mov	%g5, %l3	/* since R_Y is not used */
	mov	%g6, %l4	/* since R_TBR is not used */
	mov	%g7, %l5	/* since R_PID is not used */

	mov	%R_PSR, ARGPSR		! Save psr into global

	/* Get to window that we must underflow */
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	RETREAT_WIM( %g6, %g5, %g7 );
	restore				! This is window we trapped from
	restore				! This is the window to restore

	/* optimized out the trap_map() call */
	SUN4M_TRAP_MAP(%sp, under_tm_ok)
#ifndef NOPROC
	/* trap_map failed */
	b	under_nopte
	nop				!!
#endif

under_tm_ok:
	/* Restore the contents of the window from the stack */
	RESTORE_WINDOW_FROM_BUFFER( %sp );

under_nopte:
	save				! Get back to faulting window
	save				! Get back to trap window

	mov	%R_PSR, %psr		! Restore PSR
	/* WAIT_FOR_STATE_REG(); */
	/* Instead we can restore the globals while PSR is stabilizing: */
	mov	%l6, ARGPSR
	mov	%l3, %g5
	mov	%l4, %g6
	mov	%l5, %g7

	jmp	%R_PC
	rett	%R_NPC			!!
#endif /* SUN4M_OPTIMISE */

/*
 * trap_underflow_template() -- used as a template that gets copied into the
 * trap vector table for the underflow interrupt handler.
 */
GLOBNAME(trap_underflow_template)
	set	trap_underflow, %R_TMP1	! Make absolute address (two word insn
	jmp	%R_TMP1			! Jump to it
	mov	%psr, %R_PSR		! Save save save


#if 0
/*
 * flush_windows() -- flush the entire register file out to the stack. We
 * just do saves and restores, making the overflow and underflow handlers
 * do all the work for us.
 */
GLOBNAME(flush_windows)
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g1 );
	sub	%g1, 1, %g1
	mov	%g1, %g2

topSaveLoop:
	subcc	%g1, 1, %g1
	bnz	topSaveLoop
	save	%sp, -MINFRAME, %sp	!!

topRestoreLoop:
	subcc	%g2, 1, %g2
	bnz	topRestoreLoop
	restore				!!

	retl
	nop				!!
#endif

/* fast_flush_windows() -- flush the entire register file out to the stack. 
 * There are two ways of doing this; the first just dumbly performs
 * (NWINDOWS - 1) "save"s followed by (NWINDOWS - 1) "restore"s. Those
 * windows between the current window and the invalid one will be skipped
 * over (since they are trashable) but windows between the invalid window
 * and the one below the current window will cause a trap_overflow, which
 * will roll the stack out.
 *
 * The other way is to "restore" to each window between the current one
 * and the invalid one (not inclusive) and to flush them out by hand,
 * without a trap, in fact, without being *able* to trap. Interrupts must
 * be off and care must be taken to have no traps occur. Once the windows
 * have been tossed out to the stack, we can return to the original window
 * and set the 'window invalid mask' to point to the window below us.
 *
 * Since we cannot have traps, we must use the trap_map and trap_unmap
 * routines, and their register usage, specified above.
 */
GLOBNAME(fast_flush_windows)

	mov	%psr, %o0		! Need to disable interrupts and traps
	or	%o0, PSR_PIL, %o1
!	andn	%o1, PSR_ET, %o1
	mov	%o1, %psr
	! WAIT_FOR_STATE_REG();

	set	global_savearea, TMP

	ba	ffw_bottom
	st	%o0, [ TMP ]		!! Save the PSR for later

ffw_top:
	/* Get to window that we must overflow */
	restore

	mov	PSR_PS, ARGPSR		! Tell trap_map we are the kernel
#ifdef SUN4c
	set	trap_map, TMP		! Call trap_map( %sp )
	jmpl	TMP, RETADDR
	mov	%sp, ARGVA		!!

	cmp	RC, TM_BAD		! If trap_map failed we ignore it and
	be	ffw_nopte		! go to next window.
	nop				!!
#else
	/* optimised out trap_map call */
	SUN4M_TRAP_MAP(%sp, ffw_tm_ok)

	/* trap_map failed */
	b	ffw_nopte
	nop				!!
#endif

ffw_tm_ok:
	/* Save the contents of the window to the stack */
	SAVE_WINDOW_TO_BUFFER( %sp );

#ifdef SUN4c
	cmp	RC, TM_NADA		! We did not map it
	be	ffw_nopte		! ... so we do not unmap it
	nop				!!

	set	trap_unmap, TMP		! Call trap_unmap()
	jmpl	TMP, RETADDR
	nop				!!
#endif

ffw_nopte:
ffw_bottom:
	/*
	 * Check if we are at the edge of the world, looking off into the
	 * vastness of the invalid register.
	 */
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 );
	UNDERFLOW_TEST( %g4, %g5, %g7 );
	bz	ffw_top
	nop				!!

	LOAD( GLNAME(machine) + MI_NWINDOWS, %g6 );

	set	global_savearea, %g5	! Fetch the orignal PSR again
	ld	[ %g5 ], %g5
	and	%g5, PSR_CWP, %g7
	and	%g5, (PSR_CWP|PSR_PIL), %g4	! Save CWP & PIL for later use
	add	%g7, 1, %g7

	cmp	%g6, %g7
	be,a	ffw_cwpmax
	mov	%g0, %g7		!! Start at bottom
ffw_cwpmax:
	mov	1, %g5
	sll	%g5, %g7, %g5
	mov	%g5, %wim
	! WAIT_FOR_STATE_REG();

	mov	%psr, %g5
	andn	%g5, (PSR_CWP|PSR_PIL), %g5
	or	%g5, %g4, %g5		! %g4 still holds the cwp & pil
	mov	%g5, %psr
	WAIT_FOR_STATE_REG();

	retl
	nop				!!



#ifndef NOPROC

/*
 * Syscall related functions and data
 */

/*
 * sparc_syscall() -- trap handler for the syscall traps
 */
GLOBNAME(sparc_syscall)
	/*
	 * Since we have not done a "save" yet, we can still touch the
	 * trap window's local registers. We use this fact to set up our
	 * return address to point to the instruction after the "TA".
	 */
	add	%R_PC, 4, %R_PC		! Skip over trap insn
	add	%R_NPC, 4, %R_NPC

	PSR_LOWPIL( %R_TMP2 );

	sll	%o5, 2, %R_TMP1		! Fetch the proper routine
	set	GLNAME(sysc_routine), %R_TMP2
	add	%R_TMP1, %R_TMP2, %R_TMP2 ! Address in table in R_TMP2
	set	end_sysc_table, %R_TMP1	! Compare it with end of table
	cmp	%R_TMP1, %R_TMP2
	ble	bad_syscall_number	! Address was outside table!
	nop				!!
	ld	[ %R_TMP2 ], %R_TMP1
	tst	%R_TMP1
	be	bad_syscall_number	! In the hole
	/* Copy the input arguments to the output registers */
#ifndef SYSCALL_PROFILE
	mov	%i0, %o0		!!
#else
	LOAD( GLNAME(prof_users), %R_TMP2 );	! profiling anything?
	tst	%R_TMP2
	be	not_profiling
	nop				!!
	call	GLNAME(user_prof_sys_start)
	nop				!!
	mov	%o0, %R_TMP2		! current time, if it is profiling

not_profiling:
	mov	%i0, %o0
#endif
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	call	%R_TMP1			! Call the syscall handler
	mov	%i5, %o5		!!

#ifndef SYSCALL_PROFILE
	ba	trap_end		! Return from the trap
	mov	%o0, %i0		!! Set return code
#else
	tst	%R_TMP2
	be	trap_end		! wasn't profiling
	mov	%o0, %i0		!! Set return code

	mov	%R_TMP2, %o1
	call	GLNAME(user_prof_sys_finish)	! bill process for elapsed time
	mov	%R_PC, %o0		!!

	ba	trap_end		! Return from the trap
	nop				!!
#endif

bad_syscall_number:
	call	GLNAME(progerror)	! Knock the process on the head
	nop				!!
	ba	trap_end		! Return from the trap
	mov	%g0, %i0		!! Set return code to something
					!  it doesn't matter what.

GLNAME(sysc_routine):
	.word	GLNAME(sc_getreq)		/* 0 SC_getreq */
	.word	GLNAME(sc_putrep)		/* 1 SC_putrep */
	.word	GLNAME(sc_trans)		/* 2 SC_trans */
	.word	GLNAME(sc_timeout)		/* 3 SC_timeout */
	.word	GLNAME(sc_exitthread)		/* 4 SC_exitthread */

	.word	GLNAME(syscall_nop)		/* 5 SC_nop (test only) */

	.word 0; .word 0; .word 0; .word 0;	/* 6 - - 9 */
	.word 0; .word 0; .word 0; .word 0;	/* 10 - - 13 */

	.word	GLNAME(sc_cleanup)		/* 14 SC_sys_cleanup */
	.word	GLNAME(sc_getinfo)		/* 15 SC_getinfo */
	.word	GLNAME(sc_exitprocess)		/* 16 SC_exitprocess */
	.word	GLNAME(sc_threadswitch)		/* 17 SC_threadswitch */

	.word 0; .word 0; .word 0; .word 0; .word 0; .word 0;	/* 18 - 23 */

	.word	GLNAME(sc_seg_map)		/* 24 SC_seg_map */
	.word	GLNAME(sc_seg_unmap)		/* 25 SC_seg_unmap */
	.word	GLNAME(sc_seg_grow)		/* 26 SC_seg_grow */
	.word	GLNAME(sc_mu_trylock)		/* 27 SC_mu_trylock */
	.word	GLNAME(sc_mu_unlock)		/* 28 SC_mu_unlock */
	.word	GLNAME(sc_sys_setvec)		/* 29 SC_sys_setvec */
	.word	GLNAME(sc_sig_raise)		/* 30 SC_sig_raise */
	.word	GLNAME(sc_sys_sigret)		/* 31 SC_sys_sigret */
	.word	GLNAME(sc_sys_newthread)	/* 32 SC_sys_newthread */
	.word	GLNAME(sc_sys_getlocal)		/* 33 SC_sys_getlocal */
	.word	GLNAME(sc_sys_milli)		/* 34 SC_sys_milli */

#ifdef FLIPGRP
	.word	GLNAME(sc_grp_create)		/* 35 SC_grp_create */
	.word	GLNAME(sc_grp_join)		/* 36 SC_grp_join */
	.word	GLNAME(sc_grp_leave)		/* 37 SC_grp_leave */
	.word	GLNAME(sc_grp_send)		/* 38 SC_grp_send */
	.word	GLNAME(sc_grp_receive)		/* 39 SC_grp_receive */
	.word	GLNAME(sc_grp_reset)		/* 40 SC_grp_reset */
	.word	GLNAME(sc_grp_info)		/* 41 SC_grp_info */
	.word	GLNAME(sc_grp_set)		/* 42 SC_grp_set */
	.word	GLNAME(sc_grp_forward)		/* 43 SC_grp_forward */
#else
	.word 0; .word 0; .word 0; .word 0; .word 0;	/* 35 - 39 */
	.word 0; .word 0; .word 0; .word 0;		/* 40 - 43 */
#endif

	.word	GLNAME(sc_sys_null)		/* 44 SC_sys_null */

	.word	GLNAME(sc_thread_get_max_prio)	/* 45 SC_thread_get_max_priority */
	.word	GLNAME(sc_thread_set_priority)	/* 46 SC_thread_set_priority */
	.word	GLNAME(sc_thread_en_preempt)	/* 47 SC_thread_enable_preemption*/

	.word	GLNAME(sc_rpc_getreq)		/* 48 SC_thread_get_max_priority */
	.word	GLNAME(sc_rpc_putrep)		/* 49 SC_thread_set_priority */
	.word	GLNAME(sc_rpc_trans)		/* 50 SC_thread_enable_preemption*/

	.word	GLNAME(sc_rawflip)		/* 51 SC_rawflip */

#ifdef PROFILE
	.word	GLNAME(sc_profile)		/* 52 SC_sys_profil */
#else
	.word	0				/* 52 */
#endif

#ifdef  ACTMSG
	.word	GLNAME(sc_am_register)		/* 53 SC_am_register */
	.word	GLNAME(sc_am_unregister)	/* 54 SC_am_unregister */
	.word	GLNAME(sc_am_send)		/* 55 SC_am_send */
#else
	.word 0; .word 0; .word 0	/* 53 - - 55 */
#endif

#ifdef USER_INTERRUPTS
	.word	GLNAME(sc_await_intr)		/* 56 SC_await_intr */
#else
	.word	0				/* 56 */
#endif


end_sysc_table:

/*
 * sysc_test() and sysc_nop() are useful for testing and timing
 */

GLOBNAME(syscall_test)
	mov	SC_nop, %g1
	mov	%g0, %g2
	ta	TRAPV_SYSCALL
	nop
	retl
	nop

GLNAME(syscall_nop):
	retl
	mov	1, %o0			!! Need non-zero return code

/*
 * start() -- begin running a user process with the thread_ustate given as
 * the first argument. We sort of cheat here: the stackframe lies in the
 * area of the routine that called us, startuser(), so we just load our
 * registers with the interesting pieces of the thread_ustate, and jump into
 * the "return from trap" code.
 */
GLOBNAME(start)
	mov	%psr, %o2		! First we quickly disable interrupts
	or	%o2, PSR_PIL, %o2
	mov	%o2, %psr
	WAIT_FOR_STATE_REG();

	mov	%psr, %o5		! Insert the proper CWP into
	and	%o5, PSR_CWP, %o5	! .. the PSR provided by caller
	ld	[ %o0 + TU_PSR ], %R_PSR
	andn	%R_PSR, PSR_CWP, %R_PSR
	or	%o5, %R_PSR, %R_PSR

	mov	%psr, %o5
	and	%o5, PSR_CWP, %o5
	add	%o5, 1, %o5
	LOAD( GLNAME(machine) + MI_NWINDOWS, %g7 )
	cmp	%o5, %g7
	be,a	1f
	mov	0, %o5
1:
	mov	1, %o4
	sll	%o4, %o5, %o4
	mov	%o4, %wim
	! WAIT_FOR_STATE_REG();

	ld	[ %o0 + TU_PC ], %R_PC
	ld	[ %o0 + TU_NPC ], %R_NPC
	ld	[ %o0 + TU_Y ], %R_Y
	ld	[ %o0 + TU_PID ], %R_PID
	ld	[ %o0 + TU_FP ], %fp

	ld	[ %o0 + TU_SF + SF_IN ], %i0
	mov	0, %i1
	mov	0, %i2
	mov	0, %i3
	mov	0, %i4
	mov	0, %i5
	/* Do as if we return from a system call, only avoid going
	 * to the scheduler now.
	 */
	ba	te_nosched
	mov	-1, %i7			!!

#endif /* NOPROC */

/*
 * waitint() (ENTRY POINT) -- wait for an interrupt. There does not seem to
 * be a reliable way to do this, so we always return immediately. The 
 * upper layer can reliable check if any interrupts need processing.
 */
GLOBNAME(waitint)
	mov	%psr, %o2		! First we quickly enable traps
	andn	%o2, PSR_PIL, %o2
	mov	%o2, %psr
	WAIT_FOR_STATE_REG();
	retl
	nop				!!

/*
 * sparc_probe(addr, width, rw) -- check to see if the virtual memory address
 * given in the first argument exists and is either readable (if the third
 * arg is zero) or is  writable (if the third arg is non-zero). The trap
 * checking code in mmu_pagefault() will notice that the PC address was
 * between sparc_prbeg and sparc_prend, and will set the variable sparc_nofault
 * to zero for us if we get a trap on the address 'addr'.  We probe to a memory
 * location 'width' bytes wide.
 */
GLOBCOMM( GLNAME(sparc_nofault), 4 )

GLOBNAME(sparc_probe)
	save	%sp, -MINFRAME, %sp	! Make frame just for debugging ease

	sub	%i1, 1, %l0		! if (width & (width-1)) then width
	andcc	%i1, %l0, %g0		! is not a power of two.  This also
	bnz,a	sppr_exit		! catches width < 0.
	mov	0, %i0			!!

	cmp	%l0, 7			! if (width > 8) then error
	bg,a	sppr_exit
	mov	0, %i0			!!

	tst	%l0			! if (width > 1
	ble	2f	
	nop				!!

	andcc	%i0, %l0, %g0		! && (addr & (width - 1))
	bnz,a	sppr_exit		! then alignment is wrong!
	mov	0, %i0			!!

2:
	mov	1, %l1
	STORE( %l1, %l2, GLNAME(sparc_nofault) );
	cmp	%i2, 0
	bne	1f
	nop				!!

GLOBNAME(sparc_prbeg)
	/* Read test */
	cmp	%i1, 1
	be	rbyte
	cmp	%i1, 2			!!
	be	rhalf
	cmp	%i1, 4			!!
	be	rword
	cmp	%i1, 8			!!
	be	rdouble
	nop				!!
rbyte:
	ba	GLNAME(sparc_prend)
	ldub	[ %i0 ], %l2		!!
rhalf:
	ba	GLNAME(sparc_prend)
	lduh	[ %i0 ], %l2		!!
rword:
	ba	GLNAME(sparc_prend)
	ld	[ %i0 ], %l2		!!
rdouble:
	ba	GLNAME(sparc_prend)
	ldd	[ %i0 ], %l2		!!

1:
	/* Write test */
	cmp	%i1, 1
	be	wbyte
	cmp	%i1, 2			!!
	be	whalf
	cmp	%i1, 4			!!
	be	wword
	cmp	%i1, 8			!!
	be	wdouble
	nop				!!
wbyte:
	ba	GLNAME(sparc_prend)
	stb	%g0, [ %i0 ]		!!
whalf:
	ba	GLNAME(sparc_prend)
	sth	%g0, [ %i0 ]		!!
wword:
	ba	GLNAME(sparc_prend)
	st	%g0, [ %i0 ]		!!
wdouble:
	std	%g0, [ %i0 ]

GLOBNAME(sparc_prend)			/* End test */

	LOAD( GLNAME(sparc_nofault), %i0 );
sppr_exit:
	ret
	restore

/*
 * enable() (ENTRY POINT) -- enable interrupts
 * disable() (ENTRY POINT) -- disable interrupts
 * sparc_disable() (ENTRY POINT) -- disable interrupts (the rest of Amoeba
 *				    insists that disable is a void function
 *				    but we need the value returned by disable
 *				    in cpu.c).
 */
GLOBNAME(enable)
	ba	GLNAME(setints)
	mov	0, %o0		!!

GLOBNAME(disable)
GLOBNAME(sparc_disable)
	ba	GLNAME(setints)
	mov	PSR_PIL, %o0	!!
/*
 * setints() -- Set the "processor interrupt level" bits
 * in the psr to the value given in the first argument. This is broken into
 * three steps to avoid a timing problem with the sparc chip which leaves a
 * hole when you set both the PIL and the ET bits at once. We also return the
 * old PSR, which the caller can simply pass back on another setints() call.
 */
GLOBNAME(setints)
	and	%o0, PSR_PIL, %o5
	mov	%psr, %o3
	andn	%o3, PSR_ET, %o4
	mov	%o4, %psr
	WAIT_FOR_STATE_REG();
	andn	%o4, PSR_PIL, %o4
	or	%o4, %o5, %o4
	mov	%o4, %psr
	WAIT_FOR_STATE_REG();
	or	%o4, PSR_ET, %o4
	mov	%o4, %psr
	WAIT_FOR_STATE_REG();

	retl
	mov	%o3, %o0	!!

/* --------------------------------------------------------------------
 * Miscellaneous routines to perform all manner of boring work.
 * trap_panic() panic with a meaningless message.
 * sparc_getframep() returns a ``new'' window's frame pointer (which is
 *  the old stack pointer.
 * sparc_settbr() simply returns the TBR register.
 * sparc_setpil() sets the PIL bits of the PSR.
 * __builtin_saveregs() flushes the input register to the stack (for varargs)
 */

panic_msg:
	.ascii	"trap handler"
	.align	8
GLOBNAME(trap_panic)
	set	panic_msg, %o0
	call	GLNAME(panic), 1
	nop			!!

GLOBNAME(sparc_getframep)
	retl
	mov	%sp, %o0	!!

GLOBNAME(sparc_getfp)
	retl
	mov	%fp, %o0	!!

GLOBNAME(sparc_getwim)
	retl
	mov	%wim, %o0

GLOBNAME(sparc_getpsr)
	retl
	mov	%psr, %o0

GLOBNAME(sparc_gettbr)
	retl
	mov	%tbr, %o0	!!

GLOBNAME(sparc_settbr)
	mov	%o0, %tbr
	WAIT_FOR_STATE_REG();
	retl
	nop			!!

GLOBNAME(sparc_setpil)
	mov	%psr, %o2
	and	%o0, PSR_PIL, %o1
	andn	%o2, PSR_PIL, %o3
	or	%o1, %o3, %o0
	mov	%o0, %psr
	WAIT_FOR_STATE_REG();
	retl
	nop			!!

GLOBNAME(__builtin_saveregs)
        st      %i0, [ %fp + 68 ]
        st      %i1, [ %fp + 72 ]
        st      %i2, [ %fp + 76 ]
        st      %i3, [ %fp + 80 ]
        st      %i4, [ %fp + 84 ]
        retl
        st      %i5, [ %fp + 88 ]	!!

/*
 * The initial kernel stack, with a huge size....
 */
	.seg	"bss"

	.align	8
	GLOBBSS( GLNAME(kst_beg), 24*1024 );
	GLOBBSS( GLNAME(kst_end), 8 );

	.align	8
	GLOBBSS( global_savearea, 8*4 );
	GLOBBSS( GLNAME(kern_stack), 4 );

#ifdef STATISTICS
	GLOBBSS( GLNAME(trap_count), 4*256 );
	GLOBBSS( GLNAME(trap_mapping), 3*4 );
#endif	/* STATISTICS */

#ifndef NOPROC
	GLOBBSS( GLNAME(bad_ustack), 4 );
#endif
