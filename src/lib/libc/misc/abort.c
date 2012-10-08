/*	@(#)abort.c	1.4	96/02/27 11:11:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix-conformant abort() function */

#include <amoeba.h>
#include <module/signals.h>

void
abort()
{
	extern void (*_clean)();

        if (_clean) _clean();           /* flush all output files */

	/* Raise an exception.  If we have a catcher, it is called;
	   otherwise, it is turned into a heavy-weight signal (stun).
	   Ajax will map this exception code to SIGABRT.
	   If the catcher is stupid enough to return, the exception is
	   raised again, as for other exceptions (ad infinitum). */
	for (;;)
		sig_raise(EXC_ABT);
}
