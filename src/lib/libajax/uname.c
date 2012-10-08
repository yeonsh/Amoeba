/*	@(#)uname.c	1.3	96/02/27 10:59:36 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix uname() function */

#include <defarch.h>
#include <sys/utsname.h>

static struct utsname amoeba_identity = {
	"Amoeba",	/* sysname */
	"amoeba",	/* nodename */
	"5.3",		/* release */
	"Ajax",		/* version */
	ARCHITECTURE,	/* machine */
};

int
uname(name)
	struct utsname *name;
{
	*name = amoeba_identity;
	return 0;
}
