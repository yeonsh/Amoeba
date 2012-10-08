/*	@(#)signal.c	1.2	94/04/07 09:52:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <sys/types.h>
#include <signal.h>

typedef void (*handler)();

handler
signal(sig, proc)
	int sig;
	handler proc;
{
	struct sigaction sa;
	
	sa.sa_handler = proc;
	sigemptyset(&sa.sa_mask);
	if (proc != SIG_IGN && proc != SIG_DFL)
		sigaddset(&sa.sa_mask, sig);
	sa.sa_flags = 0;
	if (sigaction(sig, &sa, &sa) < 0)
		return (handler)(-1);
	return sa.sa_handler;
}
