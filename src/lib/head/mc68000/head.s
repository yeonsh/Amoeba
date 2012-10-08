/*	@(#)head.s	1.5	94/04/07 09:40:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#ifdef ACK_ASSEMBLER
.define .lino,.filn
.define	begtext,begdata,begbss
.define	EARRAY,ERANGE,ESET,EIDIVZ,EHEAP,EILLINS,ECASE,EBADGTO
.define	hol0,.reghp,.limhp,.trpim,.trppc
.define __thread_local,_environ,_capv,_am_sched,_am_frozen
.sect .text
.sect .rom
.sect .data
.sect .bss

! force linking in _write from libamoeba.a: it is needed for .trp
.extern _write


! runtime startof for 68020 machine


LINO_AD	= 0
FILN_AD	= 4

EARRAY	= 0
ERANGE	= 1
ESET	= 2
EIDIVZ	= 6
EHEAP	= 17
EILLINS	= 18
ECASE	= 20
EBADGTO = 27

#define FUNCMAIN	_m_a_i_n
#else
#define FUNCMAIN	_main
#endif
	SECTTEXT
	GLOBLDIR	start
begtext:
start:
	MOVL	sp, d0
	MOVL	d0, AUTODEC(sp)
	jsr	GLOBAL(__stackfix)	COM Optionally byte-swap stack
	ADDQAL	#4, sp
	MOVL	REGOFFSETTED(sp,8),GLOBAL(_environ)
					COM Save string environment pointer
	MOVL	REGOFFSETTED(sp,12),GLOBAL(_capv)
					COM and also capability env ptr
	jsr	GLOBAL(FUNCMAIN)	COM Call main
	lea	REGOFFSETTED(sp,16), sp	COM pop args
	MOVL	d0, AUTODEC(sp)		COM Push main's return value on the stack
	jsr	 GLOBAL(_exit)		COM Call exit with that as argument
					COM (This never returns)
	ADDQAL	#4, sp			COM but still ...


	GLOBLDIR	f68881_used
f68881_used:
	rts

SECTDATA
begdata:
#ifdef ACK_ASSEMBLER
	LONG 0
hol0:
.lino:
	LONG 0
.filn:
	LONG 0
.reghp:
	LONG 0
.limhp:
	LONG 0
.trppc:
	LONG 0
.trpim:
	LONG 0
#endif
	GLOBLDIR	__thread_local,_environ,_capv,_am_sched,_am_frozen
__thread_local:	LONG	-1
_environ:	LONG	0		COM String environment pointer
_capv:		LONG	0		COM Capability environment pointer
_am_sched:	LONG	0
_am_frozen:	LONG	0

#ifndef ACK_ASSEMBLER
COM 	Oops!  The Unix linker (ld) may move the first few bss
COM 	variables to the data segment when creating demand-paged
COM 	executables (-z option, 413 magic number. this is the
COM 	default).
COM 	Since Amoeba puts data and bss in different segments,
COM 	a buffer placed at the beginning of bss may cross the
COM 	data-bss segment boundary, which would cause segmentation
COM 	errors if the buffer were used as the buffer for a trans,
COM 	getreq or putrep call.
COM 	The easiest way out, short from forcing everyone to use
COM 	"ld -n", is to make sure that the first thing in bss will
COM 	never be used as a request buffer.  We do this by declaring
COM 	a dummy buffer here in head.s.

	.comm	padbuffer,8192
#endif

#ifndef GNU_ASSEMBLER
SECTBSS
begbss:
#endif
