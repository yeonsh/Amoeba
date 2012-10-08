/*	@(#)umask.c	1.4	94/04/07 10:33:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* umask() POSIX 5.3.3 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"

#ifdef __STDC__
mode_t umask(mode_t mask)
#else
mode_t umask(mask) mode_t mask;
#endif
{
  mode_t oldmask;
	
  FDINIT();     /* to get umask */
  oldmask = _ajax_umask;
  _ajax_umask = mask;
  return oldmask;
}
