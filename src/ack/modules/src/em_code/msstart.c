/*	@(#)msstart.c	1.2	94/04/06 11:19:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_msstart(cst)
	int cst;
{
	/*	start of message
	*/
	PS(ps_mes);
	CST((arith)cst);
}
