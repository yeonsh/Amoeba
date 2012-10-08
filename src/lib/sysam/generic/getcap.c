/*	@(#)getcap.c	1.2	94/04/07 11:17:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "caplist.h"

capability *
getcap(s)
char *s;
{
  extern struct caplist *capv;
  register struct caplist *cl = capv;

  while (cl->cl_name != 0)
	if (strcmp(cl->cl_name, s) == 0)
		return cl->cl_cap;
	else
		cl++;
  return 0;
}

#ifdef lint
struct caplist *capv;
#endif
