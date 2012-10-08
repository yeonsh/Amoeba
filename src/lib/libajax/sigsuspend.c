/*	@(#)sigsuspend.c	1.2	94/04/07 09:53:12 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigsuspend() POSIX 3.3.7
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sigstuff.h"
#include <signal.h>

int 
sigsuspend(mask)
	sigset_t *mask;
{
  mutex mu;
  sigset_t old_mask;
  capability sigsvrcap;
	
  /* Start signal server (this also initializes 
   * _ajax_sigignore, _ajax_sigpending and _ajax_sigblock) */
  if (_ajax_startsigsvr(&sigsvrcap) < 0)
	return -1;

  old_mask = _ajax_sigblock;
  _ajax_sigblock = *mask;
  sigdelset(&_ajax_sigblock,SIGKILL);
#ifdef _POSIX_JOB_CONTROL
  sigdelset(&_ajax_sigblock,SIGSTOP);
#endif

  /* check if we can return immediately */
  if(!_ajax_deliver())
  {
	mu_init(&mu);
	mu_lock(&mu);
	mu_trylock(&mu, -1);
	mu_unlock(&mu);
  }
  _ajax_deliver();	/* in case the handler changed the signal mask */

  _ajax_sigblock = old_mask;

  /* in case the handler generated a signal that was blocked 
   * but becomes unblocked when the original mask is restored
   */
  _ajax_deliver();

  ERR(EINTR, "sigsuspend: interrupted");
}

