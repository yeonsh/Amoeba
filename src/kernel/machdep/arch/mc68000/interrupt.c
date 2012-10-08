/*	@(#)interrupt.c	1.4	96/02/27 13:46:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "global.h"
#include "interrupt.h"

setup_interrupt(info, routine)
intrinfo *info;
void (*routine)();
{

	return setvec(info->ii_vector, routine);
}


#if !defined(NDEBUG)

int
interrupts_enabled()
{
	return ((getsr() & 0x700) == 0);
}

#endif /* NDEBUG */
