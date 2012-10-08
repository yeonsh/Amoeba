/*	@(#)sigdelset.c	1.2	94/04/07 09:52:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigdelset() POSIX 3.3.3
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "signal.h"

int 
sigdelset(pset, signo)
sigset_t *pset;
int signo;
{
  if(signo > 0 && signo < NSIG && (_SIGMASK(signo) & _GOODSIGMASK))
	*pset &= ~_SIGMASK(signo);
  else
	ERR(EINVAL,"sigdelset: illegal signal");

  return 0;
}

