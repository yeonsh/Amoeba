/*	@(#)f30abort.c	1.4	94/04/06 09:03:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
f30abort.c

Created Sept 23, 1991 by Philip Homburg
*/

#include <amoeba.h>
#include <_ARGS.h>

#include <arch_proto.h>

#define ABORT_VECTOR	232	/* I don't know why, yet */

void initabort _ARGS(( void ));

/*ARGSUSED*/
static void abort_int(sp)
int sp;
{
#ifndef SMALL_KERNEL
	ff_dump((char *) 0, (char *) 0);
#endif
	panic("ABORT button pressed");
}

void initabort _ARGS(( void ))
{
	(void) setvec(ABORT_VECTOR, abort_int);
}
