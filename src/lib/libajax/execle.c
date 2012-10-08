/*	@(#)execle.c	1.4	94/04/07 09:42:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* execle(3) library call emulation */

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
execle(char * name, ...)
#else
int
execle(name, va_alist)
	char *name;
	va_dcl
#endif	/* __STDC__ */
{
	char **cpp;
	char **environ;
	int retval;
	va_list	args;

	VA_START(args, name);
	
	for (cpp = (char **) args; *cpp++ != NULL; )
		;
	/* Immediately after the NULL char ptr, is the pointer to the environ-
	 * ment array.  Copy/cast it into a var of the appropriate type: */
	environ = *(char ***)cpp;
	retval = execve(name, (char **) args, environ);
	va_end(args);
	return retval;
}
