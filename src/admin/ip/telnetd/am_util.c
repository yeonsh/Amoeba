/*	@(#)am_util.c	1.2	96/02/27 10:15:48 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef AMOEBA

#include <stdlib.h>
#include <stdio.h>
#include <amoeba.h>
#include <module/mutex.h>
#ifdef __STDC__
#include <stdarg.h>
#define  va_dostart(ap, format)	va_start(ap, format)
#else
#include <varargs.h>
#define  va_dostart(ap, format)	va_start(ap)
#endif

#include "am_defs.h"

#ifdef __STDC__
void FatalError(char *format, ...)
#else
/*VARARGS1*/
void FatalError(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "telnetd: fatal: ");
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    /* if we already started a login session, kill it now */
    kill_session();

    exit(2);
}

static mutex print_mu;

#ifdef __STDC__
void warning(char *format, ...)
#else
/*VARARGS1*/
void warning(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    mu_lock(&print_mu);
    fprintf(stderr, "telnetd: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    mu_unlock(&print_mu);
    va_end(ap);
}

/* Dummy functions to get telnetd working on Amoeba without to many #ifdefs. */

/*ARGSUSED*/
#ifdef __STDC__
int syslog(int opt, char *msg, ...)
#else
/*VARARGS2*/
int syslog(opt, msg, va_alist) int opt; char *msg; va_dcl
#endif
{
    return 0;
}

/*ARGSUSED*/
int
openlog(name, opts, opts2)
char *name;
int   opts;
int   opts2;
{
    return 0;
}

/*ARGSUSED*/
int
closelog()
{
    return 0;
}

/*ARGSUSED*/
int
shutdown(fd, val)
int fd;
int val;
{
    return 0;
}

#endif /* AMOEBA */
