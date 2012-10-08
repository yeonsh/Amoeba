/*	@(#)pro.c	1.2	94/04/06 11:20:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_pro(pnam, l)
	char *pnam;
	arith l;
{
	/*	PRO pseudo with procedure name pnam and # of locals l
	*/
	PS(ps_pro);
	PNAM(pnam);
	COMMA();
	CST(l);
	NL();
}
