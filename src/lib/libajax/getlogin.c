/*	@(#)getlogin.c	1.3	94/04/07 09:44:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*  getlogin(3)
 *
 *  Author: Terrence W. Holm          Aug. 1988
 * 
 * last modified apr 22 93 Ron Visser
 */

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#ifndef  L_cuserid
#define  L_cuserid   9
#endif

char *getlogin()
{
  static char userid[L_cuserid];
  struct passwd *pw_entry;

  pw_entry = getpwuid(getuid());

  if (pw_entry == (struct passwd *)NULL) return((char *)NULL);

  strcpy(userid, pw_entry->pw_name);

  return(userid);
}
