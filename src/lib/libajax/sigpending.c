/*	@(#)sigpending.c	1.2	94/04/07 09:52:53 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigpending() POSIX 3.3.6
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sigstuff.h"
#include <signal.h>

int 
sigpending(set)
sigset_t *set;
{
  capability sigsvrcap;

  /* Start signal server (this also initializes
   * _ajax_sigignore, _ajax_sigpending and _ajax_sigblock) */
  if (_ajax_startsigsvr(&sigsvrcap) < 0)
	return -1;

  *set = _ajax_sigpending;
  return 0;
}

