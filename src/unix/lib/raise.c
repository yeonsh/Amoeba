/*	@(#)raise.c	1.2	94/04/07 14:07:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * Emulate the ANSI C raise() call to send a signal to the current process.
 * This is needed to shoot down programs that call trans, getreq or putrep
 * with illegal parameters.
 */

#include <signal.h>

int
raise(sig)
int sig;
{
    return kill(getpid(), sig);
}
