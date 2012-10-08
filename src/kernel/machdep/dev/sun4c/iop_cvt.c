/*	@(#)iop_cvt.c	1.2	94/04/06 09:30:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"

void
iop_cvt_io_addr(addr)
phys_bytes *	addr;
{
    /*
     * Physical addresses are 28 bits.  We set the 29th bit to make it clear
     * to setmap() that this is in I/O space.
     */
    *addr &= ~0xF0000000;
    *addr |= 0x10000000;
}
