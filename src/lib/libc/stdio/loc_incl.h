/*	@(#)loc_incl.h	1.4	96/03/15 14:07:11 */
/*
 * Copyright 1996 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * loc_incl.h - local include file for stdio library
 */

#include <stdio.h>

#ifndef fileno
#define	fileno(p)		((p)->_fd)
#endif
#define	io_testflag(p,x)	((p)->_flags & (x))

#include "_ARGS.h"

#ifdef    __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#define  const		/* const */
#endif

#if defined(__STDC__) && !defined(__SUNPRO_C)
#define LONG_DOUBLE	long double
#else
#define LONG_DOUBLE	double
#endif

int _doprnt	 _ARGS((const char *format, va_list ap, FILE *stream));
int _doscan	 _ARGS((FILE * stream, const char *format, va_list ap));
char *_i_compute _ARGS((unsigned long val, int base, char *s, int nrdigits));
char *_f_print   _ARGS((va_list *ap, int flags, char *s, char c, int precision));
void __cleanup	 _ARGS((void));

unsigned int _temp_id _ARGS((void));

FILE *popen	 _ARGS((const char *command, const char *type));
FILE *fdopen	 _ARGS((int fd, const char *mode));

#ifndef	NOFLOAT
char *_ecvt	 _ARGS((LONG_DOUBLE value, int ndigit, int *decpt, int *sign));
char *_fcvt	 _ARGS((LONG_DOUBLE value, int ndigit, int *decpt, int *sign));
char *_gcvt	 _ARGS((LONG_DOUBLE value, int ndigit, char *s, int flags));
#endif	/* NOFLOAT */

#define	FL_LJUST	0x0001		/* left-justify field */
#define	FL_SIGN		0x0002		/* sign in signed conversions */
#define	FL_SPACE	0x0004		/* space in signed conversions */
#define	FL_ALT		0x0008		/* alternate form */
#define	FL_ZEROFILL	0x0010		/* fill with zero's */
#define	FL_SHORT	0x0020		/* optional h */
#define	FL_LONG		0x0040		/* optional l */
#define	FL_LONGDOUBLE	0x0080		/* optional L */
#define	FL_WIDTHSPEC	0x0100		/* field width is specified */
#define	FL_PRECSPEC	0x0200		/* precision is specified */
#define FL_SIGNEDCONV	0x0400		/* may contain a sign */
#define	FL_NOASSIGN	0x0800		/* do not assign (in scanf) */
#define	FL_NOMORE	0x1000		/* all flags collected */
