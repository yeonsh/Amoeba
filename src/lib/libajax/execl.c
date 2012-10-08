/*	@(#)execl.c	1.3	94/04/07 09:42:53 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* execl(3) library call replacement */

#include "ajax.h"

#ifdef __STDC__
#include "stdarg.h"
#define VA_START(ap, last)	va_start(ap, last)
#else
#include "varargs.h"
#define VA_START(ap, last)	va_start(ap)
#endif  /* __STDC__ */

extern char **environ;


/*VARARGS2*/
#ifdef __STDC__
int
execl(char * name, ...)
#else
int
execl(name, va_alist)
	char *name;
	va_dcl
#endif	/* __STDC__ */
{
	int retval;
	va_list	args;

	VA_START(args, name);
	retval = execve(name, (char **) args, environ);
	va_end(args);
	return retval;
}
