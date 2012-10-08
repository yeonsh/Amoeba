/*	@(#)ttyname.c	1.3	94/04/07 09:54:53 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* ttyname() POSIX 4.7.2 
 * last modified apr 22 93 Ron Visser
 */


char *
ttyname(fildes)
int fildes;
{
  static char *termid = "/dev/tty";

  if(isatty(fildes))
	return termid;

  return 0;
}
