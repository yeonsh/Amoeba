/*	@(#)khead.S	1.6	94/04/07 13:34:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "map.h"
AS_BEGIN

GLOBLDIR _kst_beg,_kst_end
#if defined(MOTOROLA) || defined(ACE_ASSEMBLER)
	.set	_kst_beg,KERNELSPBOT
	.set	_kst_end,KERNELSP
#else
	_kst_beg = KERNELSPBOT
	_kst_end = KERNELSP
#endif

GLOBLDIR _textbegin, startup, _abort

#ifdef ACK_ASSEMBLER
.define .lino,.filn,.trpim
#endif

_textbegin:
#ifdef ROMKERNEL
	nop				COM as a reset vector, this will set
	MOVAL	#start,a0		COM the pc to start
#endif
start:
	MOVW	#0x2700, sr		COM ignore interrupts

#ifndef ROMKERNEL
COM copy the code from wherever it is to where it should be.
COM a0 is where it is now.
this:
#if defined(MOTOROLA) || defined(ACE_ASSEMBLER)
	lea	REGOFFSETTED(pc,EXPR(_textbegin-this-2)),a0
#else
#if defined(ACK_ASSEMBLER) || defined(GNU_ASSEMBLER)
	WORD  0x41fa;  WORD  EXPR(_textbegin - EXPR(this+2))
					COM where are we currently loaded?
#else
	lea	REGOFFSETTED(pc,EXPR(_textbegin-EXPR(this+2))),a0
					COM where are we currently loaded?
#endif /* ACK_ASSEMBLER */
#endif /* MOTOROLA || ACE_ASSEMBLER */
	MOVAL	#_textbegin,a1		COM where we want to be loaded
	CMPAL	a0,a1
	JEQ	go_on			COM already at 0x4000 so don't copy

	MOVL	#_edata,d0		COM calculate size of the segments
	SUBL	#_textbegin,d0
	LSRL	#2,d0			COM convert bytes to words
copyloop:
	MOVL	AUTOINC(a0),AUTOINC(a1)
	dbra	d0,copyloop
	SUBL	#0x10000, d0
	CMPL	#0xFFFFFFFF, d0
	JNE	copyloop
#ifdef ACK_ASSEMBLER
	MOVAL	#go_on, a0
	jmp	REGINDIR(a0)
#elif ACE_ASSEMBLER
      jmp     go_on.l                 COM go to the code we were running before
#else
	jmp	go_on:l			COM go to the code we were running before
#endif

go_on:
#endif /* ROMKERNEL */
	MOVAL	#KERNELSP, sp
	SUBAL	a6,a6			COM to terminate a6-chain cleanly
	MOVL	a6, AUTODEC(sp)
	jsr	GLOBAL(_PsetTc)		COM disable MMU
	TSTL	AUTOINC(sp)
	MOVL	#IVECBASE, d0
	movec	d0,vbr
        MOVEQ   #9,d0                   COM clear & enable cacr
        movec   d0,cacr                 COM ditto
	jsr	GLOBAL(_clearbss)	COM clear uninitialized data
startup:
	jsr	GLOBAL(_main)

_abort:	
	jsr	GLOBAL(_reboot)
   lp:
	JRA	lp

#ifdef COMM_LOCK
GLOBLDIR _testandset,_clrtsllock
_testandset:
	MOVAL	REGOFFSETTED(sp,4),a0
  spin:
	tas	REGINDIR(a0)
	beq	lockok
	MOVW	#0x2000,d0
  inplace:
	dbra	d0,inplace
	bra	spin

_clrtsllock:
	MOVAL	REGOFFSETTED(sp,4),a0
	CLRB	REGINDIR(a0)
  lockok:
	rts
#endif

GLOBLDIR _begin
SECTDATA
#ifdef ACK_ASSEMBLER
.lino:
        .data4  0               COM lino
.filn:
        .data4  0               COM filn
.trpim:
        .data4  0
#endif

#ifndef SMALL_KERNEL
#ifndef ROMKERNEL
	.comm paddingfordata,8192     COM to prevent data and bss in same page
#endif
#endif
	.comm _begin,2
