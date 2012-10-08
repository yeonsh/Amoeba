/*	@(#)alarm.c	1.4	94/04/07 09:42:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* alarm() POSIX 3.4.1
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sesstuff.h"

unsigned int
alarm(nsec)
unsigned int nsec;
{
  capability *session;
  int sec = nsec;

  SESINIT(session);
  if(ses_alarm(session,&sec) < 0)
	ERR(EIO, "alarm: ses_alarm failed");

  return sec;
}

