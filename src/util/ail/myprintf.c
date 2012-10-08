/*	@(#)myprintf.c	1.2	94/04/07 14:34:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ail.h"

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 *	Reimplementation of fprintf, uses bfile.c heavily.
 *	You'd better check that code if you wanna understand this.
 *	Before this code was used, I had to open and write files
 *	twice if a backpatch was made (nearly always). Profiling
 *	showed that ail spent 50% of its time in fopen() and write().
 *
 *	Recognized %-constructs: (not all have an argument associated)
 *	- c:	character
 *	- d:	int in decimal format	(D for a long)
 *	- p:	pointer format (a la 0x%lx)
 *	- s:	string
 *	- x:	int in hexadecimal format (X for a long)
 *	- b:	binary 4-byte int in native byte order (for python)
 *
 *	These are non standard:
 *	- n:	go to new line, indent
 *	- r:	Increment ErrCnt, report scan position
 *	- S:	Left justified string of length 24, padded with tabs
 *	- w:	Increment WarnCnt, report scan position
 *	- %:	print a "%"
 *	- >:	indent more
 *	- <:	indent less
 */

/*
 *	Print an integer. I really print a long, which is
 *	the same as an int on most of the machines Amoeba
 *	runs on. This way I don't need to write it twice.
 */
/* static */ void
PrintInt(h, i, base)
    handle h;
    register long i;
    int base;
{
    static char b[16];	/* Enough for ints < 2**31	*/
    static char sym[] = "0123456789abcdef";
    register char *p;	/* Where we are writing		*/
    int neg;		/* Is it negative?		*/
    if (neg = i<0) i = -i;
    p = b + sizeof(b);	/* Start writing the last chars	*/
    do {		/* At least print one '0'	*/
	*--p = sym [i % base];
	i /= base;
    } while (i > 0);
    if (neg) *--p = '-';
    AddBuf(h, p, b + sizeof(b) - p);
} /* PrintInt */

/*
 *	Produce an indication on where the scanner is
 */
/* static */ void
ScanPos(h)
    handle h;
{
    if (LineNumber == 0) {	/* Generators from cmd-line	*/
	AddBuf(h, "commandline", 11);
    } else {
	AddBuf(h, FileName, strlen(FileName));
	AddChar(h, ' ');
	PrintInt(h, (long) LineNumber, 10);
    }
    if (genname != NULL) {	/* Some code generator is active */
	AddBuf(h, " (", 2);
	AddBuf(h, genname, strlen(genname));
	AddBuf(h, "):", 2);
    } else AddChar(h, ':');
} /* ScanPos */

/*
 *	The worst part: the varargs routine, or the stdarg
 *	routine if ail is compiled with an ANSI compiler.
 *	Negative handles refer to standard filedescriptors,
 *	so -2 is stderr, and -1 is stdout.
 */

#if __STDC__

void
mypf(handle h, char *fmt, ...)	/* va_alist not used in ANSI	*/
{
    va_list rest;
    char *todo;
    va_start(rest, fmt);	/* Is considered a statement	*/

#else /* __STDC__ */

/*VARARGS2*/
void
mypf(h, fmt, va_alist)
    handle h;
    register char *fmt;		/* Format string		*/
    va_dcl			/* Read varargs manual		*/
{	/*}*/
    va_list rest;
    char *todo;
    va_start(rest);		/* Is considered a statement	*/

#endif /* __STDC__ */

    /* stackchk(); */
    /*
     *	Don't write to non-standard files if an error occurred
     */
    if (ErrCnt && h >= 0) return;

    /*
     *	Try to replace several AddChars by one AddBuf.
     *	Must remember which chars we didn't print yet.
     */
    todo = fmt;
    while (*fmt != '\0') {
	if (*fmt != '%') ++fmt;
	else {
	    if (todo != fmt) {	/* Emit buffered chars	*/
		AddBuf(h, todo, fmt - todo);
		todo = fmt + 2;	/* Past the 'd' of a %d	*/
	    }
	    switch (*++fmt) {
	    case 'b': {
		long tmp;
		tmp = (long) va_arg(rest, long);
		AddBuf(h, (char *) &tmp, sizeof(long));
		break;
	    }
	    case 'c':
		AddChar(h, va_arg(rest, int));
		break;
	    case 'd':
		PrintInt(h, (long) (va_arg(rest, int)), 10);
		break;
	    case 'D':
		PrintInt(h, (va_arg(rest, long)), 10);
		break;
	    case 'n':		/* indent				*/
		Indent(h);
		break;
	    case 'p':
		AddBuf(h, "0x", 2);
		PrintInt(h, (long) (va_arg(rest, char *)), 16);
		break;
	    case 'r':		/* Error, report scanner position	*/
		++ErrCnt;
		ScanPos(h);
		break;
	    case 's': {
		register char *tmp;
		tmp = va_arg(rest, char *);
		AddBuf(h, tmp, strlen(tmp));
		break;
	    }
	    case 'S': {		/* LeftAligned string of lenght 24	*/
		int len;
		register char *tmp;
		tmp = va_arg(rest, char *);
		len = strlen(tmp);
		AddBuf(h, tmp, len);
		if (len < 24) AddChar(h, '\t');
		if (len < 16) AddChar(h, '\t');
		if (len < 8) AddChar(h, '\t');
		break;
	    }
	    case 'w':		/* Warning; report scanner position	*/
		++WarnCnt;
		ScanPos(h);
		AddBuf(h, " (warning)", 10);
		break;
	    case 'x':		/* Integer in hexadecimal		*/
		PrintInt(h, (va_arg(rest, int)), 16);
		break;
	    case 'X':		/* Long integer in hexadecimal		*/
		PrintInt(h, (va_arg(rest, long)), 16);
		break;
	    case '%':		/* Escaped '%'	*/
		AddChar(h, '%');
		break;
	    case '<':		/* back-indent	*/
		IndentChange(h, -1);
		break;
	    case '>':		/* forw-indent	*/
		IndentChange(h, 1);
		break;
	    default:
		abort();
	    }
	    todo = ++fmt;
	}
    }
    if (todo != fmt) AddBuf(h, todo, fmt - todo);
    va_end(rest);
} /* mypf */
