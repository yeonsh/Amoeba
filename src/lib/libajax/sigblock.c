/*	@(#)sigblock.c	1.2	94/04/07 09:52:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigblock(2) system call emulation */
/* TO DO: DO IT */

#include "ajax.h"

int
sigblock(mask)
	long mask;
{
	ERR(EIO, "sigblock: not implemented");
}
