/*	@(#)as10.S	1.2	94/04/06 08:58:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "assyntax.h"
AS_BEGIN
SECTTEXT

GLOBLDIR _tctlb,_tctlw,_tctll,_fctlb,_fctlw,_fctll

_tctlb:
	MOVAL	REGOFFSETTED(sp,4),a0
	MOVB	REGOFFSETTED(sp,11),d0
	MOVESB	d0,REGINDIR(a0)
	rts
	
_tctlw:
	MOVAL	REGOFFSETTED(sp,4),a0
	MOVW	REGOFFSETTED(sp,10),d0
	MOVESW	d0,REGINDIR(a0)
	rts

_tctll:
	MOVAL	REGOFFSETTED(sp,4),a0
	MOVL	REGOFFSETTED(sp,8),d0
	MOVESL	d0,REGINDIR(a0)
	rts

_fctlb:
	MOVAL	REGOFFSETTED(sp,4),a0
	CLRL	d0
	MOVESB	REGINDIR(a0),d0
	rts

_fctlw:
	MOVAL	REGOFFSETTED(sp,4),a0
	CLRL	d0
	MOVESW	REGINDIR(a0),d0
	rts

_fctll:
	MOVAL	REGOFFSETTED(sp,4),a0
	CLRL	d0
	MOVESL	REGINDIR(a0),d0
	rts
