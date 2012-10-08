/*	@(#)sigprocmask.c	1.2	94/04/07 09:52:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigprocmask() POSIX 3.3.5
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sigstuff.h"
#include <signal.h>

int 
sigprocmask(how, set, oset)
int how;
sigset_t *set;
sigset_t *oset;
{
  sigset_t old_mask;
  capability sigsvrcap;

  /* Start signal server (this also initializes 
  * _ajax_sigignore, _ajax_sigpending and _ajax_sigblock) */
  if (_ajax_startsigsvr(&sigsvrcap) < 0)
	return -1;

  old_mask = _ajax_sigblock;
  if(set == NULL)
  {
	if(oset != NULL)
		*oset = old_mask;
	return 0;
  }

  switch(how){
  case SIG_BLOCK:
	_ajax_sigblock |= *set;
	break;
  case SIG_UNBLOCK:
	_ajax_sigblock ^= (_ajax_sigblock & *set);
	break;
  case SIG_SETMASK:
	_ajax_sigblock = *set;
	break;
  default:
	ERR(EINVAL, "sigprocmask: unsupported action");
  }
  sigdelset(&_ajax_sigblock,SIGKILL);
#ifdef _POSIX_JOB_CONTROL
  sigdelset(&_ajax_sigblock,SIGSTOP);
#endif
 
  /* check pending calls */
  _ajax_deliver();

  if(oset != NULL)
	*oset = old_mask;

  return 0;
}
