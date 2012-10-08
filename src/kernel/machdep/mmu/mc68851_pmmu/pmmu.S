/*	@(#)pmmu.S	1.2	94/04/06 09:41:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

COM
COM Interface routines for PMMU instructions
COM To allow for MC68030 only control alterable effective addresses
COM should be used. Just to be on the safe side we only use ax@.
AS_BEGIN
GLOBLDIR _PsetSrp,_PsetCrp,_PsetTc,_Pflushr,_Pflusha,_PgetTc,_Ptest,_Ploadw

_PsetSrp:
	MOVAL	REGOFFSETTED(sp,4),a0	COM address of new SRP
	WORD	0xF010			COM pmove a0@,srp
	WORD	0x4800
	rts

_PsetCrp:
	MOVAL	REGOFFSETTED(sp,4),a0	COM address of new CRP
	WORD	0xF010			COM pmove a0@,crp
	WORD	0x4C00
	rts

_PsetTc:
	lea	REGOFFSETTED(sp,4),a0	COM address of new TC
	WORD	0xF010			COM pmove a0@,tc
	WORD	0x4000
	rts

_PgetTc:
	TSTL	AUTODEC(sp)		COM reserve space for result
	WORD	0xF017			COM pmove tc,sp@
	WORD	0x4200
	MOVL	AUTOINC(sp),d0		COM return result
	rts

_Pflushr:				COM not available on MC68030 !
	MOVAL	REGOFFSETTED(sp,4),a0	COM address of crp to flush
	WORD	0xF010			COM pflushr a0@
	WORD	0xA000
	rts

_Pflusha:
	WORD	0xF000
	WORD	0x2400
	rts

_Ptest:
	MOVAL	REGOFFSETTED(sp,4),a0	COM address to ptest
	MOVL	REGOFFSETTED(sp,8),d0	COM function code
	WORD	0xF010			COM ptestr d0,a0@,#7,a1
	WORD	0x9F28
	TSTL	AUTODEC(sp)		COM reserve space for result
	WORD	0xF017			COM pmove psr,sp@
	WORD	0x6200
	MOVL	AUTOINC(sp),d0		COM return result
	MOVAL	REGOFFSETTED(sp,12),a0	COM address to put descriptor pointer
	MOVL	a1,REGINDIR(a0)
	rts

_Ploadw:
	MOVAL	REGOFFSETTED(sp,4),a0	COM address to ploadw
	WORD	0xF010			COM ploadw #1,a0@
	WORD	0x2011
	rts
