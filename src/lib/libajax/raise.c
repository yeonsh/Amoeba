/*	@(#)raise.c	1.3	94/04/07 09:49:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* last modified may 14 93 Ron Visser */
#include <amoeba.h>
#include <module/signals.h>
#include <signal.h>
#include "sigstuff.h"

/*
 * Standard C compatible raise() function.
 */

int
raise(sig)
int sig;
{
    handler cur_handler;

    if (sigismember(&_ajax_sigblock,sig)) {
	sigaddset(&_ajax_sigpending,sig);
	return 0;
    }

    /* get current signal handler */
    cur_handler = signal(sig, SIG_DFL);

    if (cur_handler == SIG_DFL) {
	_ajax_sigself(sig); 	      /* default action: stun ourselves */
	return -1;		      /* it's an error if we still live here */
    } else {
	(void) signal(sig, cur_handler);  /* restore signal handler */
	sig_raise(sig);		      /* and call it */
	return 0;
    }
}

