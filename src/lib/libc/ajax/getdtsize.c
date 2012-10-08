/*	@(#)getdtsize.c	1.2	94/04/07 10:28:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getdtablesize(2) system call emulation */

#include "ajax.h"
#include "fdstuff.h"

int
getdtablesize()
{
	return NOFILE;
}
