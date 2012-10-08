/*	@(#)assert.c	1.7	94/04/06 10:05:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef __STDC__
#include "stdarg.h"
#define VA_LIST         va_list
#define VA_ALIST
#define VA_DCL
#define VA_START( ap, last )    va_start( ap, last )
#define VA_END          va_end
#define VA_ARG          va_arg
#else
#include "varargs.h"
#define VA_LIST         va_list
#define VA_ALIST        , va_alist
#define VA_DCL          va_dcl
#define VA_START( ap, last )    va_start( ap )
#define VA_END          va_end
#define VA_ARG          va_arg
#endif  /* __STDC__ */

#include "sys/proto.h"

#ifdef __STDC__
panic(char *fmt, ...)
#else
/*VARARGS1*/
panic(fmt VA_ALIST)
char *fmt;
VA_DCL
#endif /* __STDC__ */
{
    char *	doprnt();
    VA_LIST	args;

    disable();
#ifndef NO_OUTPUT_ON_CONSOLE
    console_enable(1);
#endif
    printf("Panic: ");

    VA_START(args, fmt);
    (void) doprnt((char *) 0, (char *) 0, fmt, args);
    VA_END(args);
    
    printf("\n");
    stacktrace();
    helloworld();
    exitkernel();
}

#ifndef NDEBUG

void
badassertion(file, line)
char *file;
int line;
{
#ifndef SMALL_KERNEL
#ifdef DEBUG
    rpc_dump(0,0);
#ifdef FLIPGRP
    grp_dump(0,0);
#endif /* FLIPGRP */
#endif /* DEBUG */
#if FLIP_DEBUG
    ff_dump(0, 0);
    ffstatistics(0, 0);
#endif /* TCPIP_DEBUG */
#endif
    panic("assertion failed in %s, line %d", file, line);
    /*NOTREACHED*/
}

void
badassertionN(n, file, line)
int n;
char *file;
int line;
{
#ifdef DEBUG
    rpc_dump(0,0);
#endif /* DEBUG */
    panic("assertion %d failed in %s, line %d", n, file, line);
    /*NOTREACHED*/
}

void
badcompare(file, line, a, b)
char *file;
int line;
long a;
long b;
{
#ifdef DEBUG
    rpc_dump(0,0);
#endif /* DEBUG */
    panic("comparison failed in %s, line %d (%ld, %d)", file, line, a, b);
    /*NOTREACHED*/
}

#endif
