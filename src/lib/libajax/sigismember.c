/*	@(#)sigismember.c	1.2	94/04/07 09:52:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigismember() POSIX 3.3.3
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "signal.h"

int 
sigismember(pset, signo)
sigset_t *pset;
int signo;
{
  if(signo > 0 && signo < NSIG && (_SIGMASK(signo) & _GOODSIGMASK))
	return ((*pset & _SIGMASK(signo)) != 0);
  else
	ERR(EINVAL,"sigismember: illegal signal");
}

