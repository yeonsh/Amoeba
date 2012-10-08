/*	@(#)as20.S	1.2	94/04/06 08:59:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "assyntax.h"
AS_BEGIN
SECTTEXT

GLOBLDIR _tcacr

_tcacr:
	MOVL	REGOFFSETTED(sp,4),d0
	movec	d0,cacr
	rts
