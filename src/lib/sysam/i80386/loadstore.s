/*	@(#)loadstore.s	1.2	94/04/07 11:20:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"

AS_BEGIN

	GLOBL GLNAME(__ldstub)
GLNAME(__ldstub):
	MOV_L	(REGOFF(4,ESP),EAX)
	BTS_L	(CONST(0),REGIND(EAX))
	JC	(locked)
	XOR_L	(EAX,EAX)
	RET
locked: MOV_L	(CONST(1),EAX)
	RET
