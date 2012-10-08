/*	@(#)endarg.c	1.2	94/04/06 11:17:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "em_private.h"


CC_endnarg()
{
	/*	END pseudo of procedure with unknown # of locals
	*/
	PS(ps_end);
	CCEND();
	NL();
}
