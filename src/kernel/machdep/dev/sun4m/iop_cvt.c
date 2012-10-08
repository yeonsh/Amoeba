/*	@(#)iop_cvt.c	1.2	94/04/06 09:37:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"

/*ARGSUSED*/
void
iop_cvt_io_addr(addr)
phys_bytes *	addr;
{
    /* No conversion required for sun4m */
}
