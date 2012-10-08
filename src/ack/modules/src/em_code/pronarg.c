/*	@(#)pronarg.c	1.2	94/04/06 11:20:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_pronarg(pnam)
	char *pnam;
{
	/*	PRO pseudo with procedure name pnam and unknown # of locals
	*/
	PS(ps_pro);
	PNAM(pnam);
	COMMA();
	CCEND();
	NL();
}
