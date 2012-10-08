/*	@(#)assyntax.h	1.5	96/02/27 10:30:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: assyntax.h -- While we do not need the cpp voodoo to support the
 * ACK assembler, this does make a convienient place to put those nice
 * macros to make assembly look less like assembly.
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   (Univerity of Hawai'i, Manoa) November 1990
 */

#ifndef __ASSYNTAX_H__
#define __ASSYNTAX_H__

/* Generic register usage */
#define R_FP i6
#define R_SP o6

/* Bindings specific to the fault frame of a trap handler */
#define  R_PSR l0
#define   R_PC l1
#define  R_NPC l2
#define  R_TBR l3
#define    R_Y l4
#define  R_PID l5
#define R_TMP1 l6
#define R_TMP2 l7

/* Cruft for ``machine independant'' assembly sources */
#ifdef __STDC__
#define	_CONCAT(a,b)	a##b
#else
#define	_CONCAT(a,b)	a/**/b
#endif

#define	SEG_TEXT	.seg "text"
#define	SEG_DATA	.seg "data"
#define	SEG_BSS		.seg "bss"
#define	AS_BEGIN	SEG_TEXT
#define GLOBLDIR	.global
#define GLOBAL( x )	x

#ifdef NO_UNDERSCORE
#define	GLNAME(name)	name
#else
#define	GLNAME(name)	_CONCAT(_,name)
#endif /* NO_UNDERSCORE */

#define GLOBNAME(n)	GLOBLDIR GLNAME(n); GLNAME(n):

#define GLOBTAG( name )	.global name; name:
#define GLOBCOMM( name, size )	.global name; .comm name, size
#define	GLOBBSS( name, size ) 		\
	.global name;			\
	.seg "bss";			\
	name: .skip size;		\
	.seg "text"; 

/*
 * Some macros for optimization and readability
 */
#define STORE( what, via, where )		\
	sethi	%hi( where ), via;		\
	st	what, [via+%lo(where)]

#define LOAD( what, where )			\
	sethi	%hi(what), where;		\
	ld	[where+%lo(what)], where

#define LOADHALF( what, where )			\
	sethi	%hi(what), where;		\
	ldsh	[where+%lo(what)], where
	
/*
 * When we modify any ``state'' register, the sparc requires that next
 * three instructions not depend on the contents of that registers. We
 * ensure this (in most circumstances) by simply doing three NOPs.
 * Hell, we'll do ten just for luck.....
 */
#define WAIT_FOR_STATE_REG()			\
	nop; nop; nop; /*nop; nop; nop; nop; nop; nop; nop; */

/*
 * Set the PIL to the given value after turning traps off. Done as
 * two steps to avoid crufty chip bug where lowering the PIL while
 * disabling interrupts (or raising the PIL while enabling interrupts)
 * leaves a timing hole where lower priority interrupts can get through.
 * We must first disable traps (so nobody can bother me), and then set
 * the PIL to given value.
 */
#define	PSR_SETPIL( valreg, usereg1, usereg2 )	\
	and	valreg, PSR_PIL, usereg2;	\
	mov	%psr, usereg1;			\
	andn	usereg1, PSR_ET, usereg1;	\
	mov	usereg1, %psr;			\
	WAIT_FOR_STATE_REG();			\
	andn	usereg1, PSR_PIL, usereg1;	\
	or	usereg1, usereg2, usereg1;	\
	mov	usereg1, %psr;			\
	WAIT_FOR_STATE_REG();			\
	or	usereg1, PSR_ET, usereg1;	\
	mov	usereg1, %psr;			\
	WAIT_FOR_STATE_REG();

/*
 * We want to enable traps and raise the PIL to all-ones without tickling
 * the chip bug discussed above. To do this we first raise the PIL and
 * then enable traps.
 */
#define	PSR_HIGHPIL( reg )			\
	mov	%psr, reg;			\
	or	reg, PSR_PIL, reg;		\
	mov	reg, %psr;			\
	WAIT_FOR_STATE_REG();			\
	or	reg, PSR_ET, reg;		\
	mov	reg, %psr;			\
	WAIT_FOR_STATE_REG()

/*
 * This time we want to enable traps and lower PIL to zero. This could
 * be done as one step, but how knows what bug that will tickled.
 */
#define	PSR_LOWPIL( reg )			\
	mov	%psr, reg;			\
	andn	reg, PSR_PIL, reg;		\
	mov	reg, %psr;			\
	WAIT_FOR_STATE_REG();			\
	or	reg, PSR_ET, reg;		\
	mov	reg, %psr;			\
	WAIT_FOR_STATE_REG()

/*
 * Advance the Window Invalid Mask, to enable us to "restore" to the
 * previously invalid window, which we have just made valid and usable.
 * The idea is that wim = (( wim >> 1 ) | ( wim << nwindows-1 ))
 */
#define	ADVANCE_WIM( tmp1, tmp2, nwindows )	\
	mov	%wim, tmp1;			\
	sub	nwindows, 1, nwindows;		\
	srl	tmp1, 1, tmp2;			\
	sll	tmp1, nwindows, tmp1;		\
	or	tmp1, tmp2, tmp1;		\
	mov	tmp1, %wim;			\
	WAIT_FOR_STATE_REG();

/*
 * Retreat the WIM, exposing another window to the harsh reality of user
 * registers.
 */
#define	RETREAT_WIM( tmp1, tmp2, nwindows )	\
	mov	%wim, tmp1;			\
	sub	nwindows, 1, nwindows;		\
	sll	tmp1, 1, tmp2;			\
	srl	tmp1, nwindows, tmp1;		\
	or	tmp1, tmp2, tmp1;		\
	mov	tmp1, %wim;			\
	WAIT_FOR_STATE_REG();

/*
 * Check if we are about to underflow, that is, if doing a "restore"
 * or "rett" would cause an underflow trap. Condition codes will be
 * "non-zero" (ie. bnz) if we would trap-underflow.
 */
#define	UNDERFLOW_TEST( tmp1, tmp2, nwindows )	\
	mov	%psr, tmp1;			\
	and	tmp1, PSR_CWP, tmp1;		\
	add	tmp1, 1, tmp1;			\
	cmp	tmp1, nwindows;			\
	be,a	9f;				\
	mov	0, tmp1;			\
9:						\
	mov	1, tmp2;			\
	sll	tmp2, tmp1, tmp2;		\
	mov	%wim, tmp1;			\
	andcc	tmp1, tmp2, %g0;

/*
 * Check if we are *in* an invalid window, requiring us to perform
 * overflow handling before continuing.
 */
#define INVALID_TEST( tmp1, tmp2 )		\
	mov	%psr, tmp1;			\
	and	tmp1, PSR_CWP, tmp1;		\
	mov	1, tmp2;			\
	sll	tmp2, tmp1, tmp1;		\
	mov	%wim, tmp2;			\
	andcc	tmp1, tmp2, %g0;

/*
 * Save and restore global registers to/from the buffer given
 */
#define	SAVE_GLOBALS_TO_BUFFER( bufp )		\
	std	%g0, [ bufp ];			\
	std	%g2, [ bufp + 8 ];		\
	std	%g4, [ bufp + 16 ];		\
	std	%g6, [ bufp + 24 ]

#define	RESTORE_GLOBALS_FROM_BUFFER( bufp )	\
	ldd	[ bufp +  0 ], %g0;		\
	ldd	[ bufp +  8 ], %g2;		\
	ldd	[ bufp + 16 ], %g4;		\
	ldd	[ bufp + 24 ], %g6

/*
 * Save and restore a window of register to/from the buffer given
 */
#define	SAVE_WINDOW_TO_BUFFER( bufp )		\
	std	%l0, [ bufp +  0 ];		\
	std	%l2, [ bufp +  8 ];		\
	std	%l4, [ bufp + 16 ];		\
	std	%l6, [ bufp + 24 ];		\
	std	%i0, [ bufp + 32 ];		\
	std	%i2, [ bufp + 40 ];		\
	std	%i4, [ bufp + 48 ];		\
	std	%i6, [ bufp + 56 ]

#define	RESTORE_WINDOW_FROM_BUFFER( bufp )	\
	ldd	[ bufp +  0], %l0;		\
	ldd	[ bufp +  8 ], %l2;		\
	ldd	[ bufp + 16 ], %l4;		\
	ldd	[ bufp + 24 ], %l6;		\
	ldd	[ bufp + 32 ], %i0;		\
	ldd	[ bufp + 40 ], %i2;		\
	ldd	[ bufp + 48 ], %i4;		\
	ldd	[ bufp + 56 ], %i6

#endif /* __ASSYNTAX_H__ */
