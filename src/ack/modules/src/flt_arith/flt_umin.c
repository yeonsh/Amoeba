/*	@(#)flt_umin.c	1.2	93/01/15 17:13:06 */
/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/


#include "flt_misc.h"

flt_umin(e)
	flt_arith *e;
{
	/*	Unary minus
	*/
	flt_status = 0;
	e->flt_sign = ! e->flt_sign;
}
