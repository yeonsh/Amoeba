/*	@(#)setjmp.s	1.2	94/04/07 10:41:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
AS_BEGIN

#ifdef ACK_ASSEMBLER
/* This piece of code is only valid for the ACK ANSI C-Compiler.
 * As can be seen in "setjmp.h", in this case "__setjmp" should be defined,
 * rather than "setjmp".
 */

SECTBSS
.comm _gtobuf,12

GLOBLDIR ___setjmp
SECTTEXT
___setjmp:
	link	a6,#0
#ifdef _POSIX_SOURCE
	MOVL	REGOFFSETTED(a6,8),a0
	MOVL	REGOFFSETTED(a6,12),REGOFFSETTED(a0,16)
	JEQ	dontset
	MOVL	REGOFFSETTED(a6,8),a0
	pea	REGOFFSETTED(a0,12)
	JSR	GLOBAL(___newsigset)
	ADDL	#4,sp
dontset:
#endif
	MOVL	REGOFFSETTED(a6,8),a0
	MOVL	REGINDIR(a6),AUTODEC(sp)
	pea	REGOFFSETTED(a6,8)
	MOVL	REGOFFSETTED(a6,16),AUTODEC(sp)
	MOVL	AUTOINC(sp),AUTOINC(a0)
	MOVL	AUTOINC(sp),AUTOINC(a0)
	MOVL	AUTOINC(sp),AUTOINC(a0)
	CLRL	d0
	unlk	a6
	rts

GLOBLDIR _longjmp
_longjmp:
	link	a6,#0
#ifdef _POSIX_SOURCE
	MOVL	REGOFFSETTED(a6,8),a0
	TSTL	REGOFFSETTED(a0,16)
	JEQ	dontset2
	pea	REGOFFSETTED(a0,12)
	JSR	GLOBAL(___oldsigset)
	ADDL	#4,sp
dontset2:
#endif
	MOVL	#_gtobuf,a0
	MOVL	REGOFFSETTED(a6,8),a1
	MOVL	AUTOINC(a1),AUTOINC(a0)
	MOVL	AUTOINC(a1),AUTOINC(a0)
	MOVL	AUTOINC(a1),AUTOINC(a0)
	MOVL	REGOFFSETTED(a6,12),d0
	bne	skip	
	MOVL	#1,d0		COM don't return 0, even if the longjmp says so
skip:
	MOVL	#_gtobuf,a0
	MOVL	REGOFFSETTED(a0,8),a6
	MOVL	REGOFFSETTED(a0,4),sp
	MOVL	REGINDIR(a0),a0
	jmp	REGINDIR(a0)

#else /* !ACK_ASSEMBLER */

GLOBLDIR _setjmp
GLOBLDIR _longjmp
_setjmp:
 	MOVAL	REGOFFSETTED(sp,4),a0
 	MOVL	REGINDIR(sp),REGINDIR(a0)
 	CLRL	REGOFFSETTED(a0,8)
 	MOVEML	d2-d7/a2-sp,REGOFFSETTED(a0,12)
 	CLRL	d0
 	rts
_longjmp:
 	MOVAL	REGOFFSETTED(sp,4),a0
 	MOVL	REGOFFSETTED(sp,8),d0
 	bne	.L1
 	MOVEQ	#1,d0
.L1:
 	MOVAL	REGINDIR(a0),a1
 	MOVEML	REGOFFSETTED(a0,12),d2-d7/a2-sp
 	ADDQAL	#4,sp
 	jmp	REGINDIR(a1)

#endif /* ACK_ASSEMBLER */

