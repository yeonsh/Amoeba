/*	@(#)execvp.c	1.3	96/02/27 10:59:04 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ajax.h"

#ifdef __STDC__
#include "stdarg.h"
#define VA_START(ap, last)	va_start(ap, last)
#else
#include "varargs.h"
#define VA_START(ap, last)	va_start(ap)
#endif  /* __STDC__ */



/*VARARGS2*/
#ifdef __STDC__
int
execlp(char * name, ...)
#else
int
execlp(name, va_alist)
char *name;
va_dcl
#endif	/* __STDC__ */
{
	int retval;
	va_list	args;

	VA_START(args, name);
	retval = execvp(name, (char **) args);
	va_end(args);
	return retval;
}

execvp(name, argv)
char *name, **argv;
{
	char *path = getenv("PATH");
	register char *c = "";
	char progname[1024];

	if (path == 0) path = ":/bin:/usr/bin";
	if (! strchr(name, '/')) c = path;

	do {
		register char *p = progname;
		register char *n = name;
		char *c1 = c;

		while (*c && *c != ':') {
			*p++ = *c++;
		}
		if (c != c1) *p++ = '/';
		if (*c) c++;
		while (*n) *p++ = *n++;
		*p = 0;

		execv(progname, argv);
	} while (*c);
	return(-1);
}
