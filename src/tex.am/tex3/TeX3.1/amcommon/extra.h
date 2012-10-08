/*	@(#)extra.h	1.1	91/11/21 13:44:25 */
/* Main include file for this implementation.  Everybody includes this.  */

#ifndef EXTRA_H
#define EXTRA_H

#include <stdio.h>
#include "site.h"
#include "common.h"

/* Path searching.  The `...PATH' constants are used both as indices and
   just as enumeration values.  Their values must match the
   initialization of `env_var_names' in extra.c.  The `...PATHBIT'
   constants are used in the argument to `setpaths'.  */
#define BIBINPUTPATH	0
#define BIBINPUTPATHBIT (1 << BIBINPUTPATH)
#define GFFILEPATH	1
#define GFFILEPATHBIT (1 << GFFILEPATH)
#define MFBASEPATH	2
#define MFBASEPATHBIT (1 << MFBASEPATH)
#define MFINPUTPATH	3
#define MFINPUTPATHBIT (1 << MFINPUTPATH)
#define MFPOOLPATH	4
#define MFPOOLPATHBIT (1 << MFPOOLPATH)
#define PKFILEPATH	5
#define PKFILEPATHBIT (1 << PKFILEPATH)
#define TEXFORMATPATH	6
#define TEXFORMATPATHBIT (1 << TEXFORMATPATH)
#define TEXINPUTPATH	7
#define TEXINPUTPATHBIT (1 << TEXINPUTPATH)
#define TEXPOOLPATH	8
#define TEXPOOLPATHBIT (1 << TEXPOOLPATH)
#define TFMFILEPATH	9
#define TFMFILEPATHBIT (1 << TFMFILEPATH)
#define VFFILEPATH     10
#define VFFILEPATHBIT (1 << VFFILEPATH)

/* Globally needed routines we can implement as macros.  */

/* Absolute value.  Without the casts to integer here, the Ultrix and
   AIX compilers (at least) produce bad code (or maybe it's that I don't
   understand all the casting rules in C) for tests on memory fields. 
   Specifically, a test in diag_round (in Metafont) on a quarterword
   comes out differently without the cast, thus causing the trap test to
   fail.  (A path at line 86 is constructed slightly differently).  */
#define	abs(x)		((integer) (x) >= 0 ? (integer) (x) : (integer) -(x))

#define	chr(x)		(x)
#define	decr(x)		--(x)
#define eof(f)		test_eof (f)
#define	fabs(x)		((x) >= 0.0 ? (x) : -(x))
#define flush(f)	(void) fflush (f)
#define	Fputs(f, s)	(void) fputs (s, f)  /* fixwrites outputs this.  */
#define	incr(x)		++(x)
#define	input3ints(a, b, c)  zinput3ints (&a, &b, &c)
#define	odd(x)		((x) % 2)
#define ord(x)		(x)
#define printreal(r, n, m)  fprintreal (stdout, r, n, m)
#define	putbyte(x, f)	putc ((char) (x) & 255, f)
#define read(f, b)	((b) = getc (f))
#define	readln(f)	{ register c; \
                          while ((c = getc (f)) != '\n' && c != EOF); }
#define	round(x)	zround ((double) (x))

/* Open files for reading and writing.  */
#define	reset(f, n) \
  ((f) ? fclose (f) : 0), (f) = checked_fopen ((char *) (n), "r")
#define rewrite(f, n) \
  (f) = checked_fopen ((char *) (n), "w")

#define	toint(x)	((integer) (x))

/* Pascal's predefined `trunc' routine.  */
#define trunc(x)	((integer) (x))

/* ``Unix exit'', since WEB already defines the symbol `exit'.  */
#define	uexit		exit

/* For throwing away input from the file F.  */
#define vgetc(f)	(void) getc (f)

/* If we don't care that strcpy(3) returns A.  */
#define vstrcpy(a, b)	(void) strcpy (a, b)

/* Write out elements START through END of BUF to the file F.  */
#define writechunk(f, buf, start, end) \
  (void) fwrite (&buf[start], sizeof (buf[start]), end - start + 1, f)

/* Like fseek(3), but cast the arguments and ignore the return value.  */
#define checkedfseek(f, n, w)  (void) fseek(f, (long) n, (int) w)


/* C doesn't distinguish between text files and other files.  */
typedef FILE *text, *file_ptr;

/* For some initializations of constant strings.  */
typedef char *ccharpointer;

/* We need one global variable.  */
extern integer argc;

/* Routines in extra.c and main.c.  */
#ifdef ANSI
extern void argv (int, char[]);
extern FILE *checked_fopen (char *, char *);
extern boolean eoln (FILE *);
extern void fprintreal (FILE *, double, int, int);
extern integer inputint (FILE *);
extern char *xmalloc (unsigned), *xrealloc (char *, unsigned);
extern integer zround (double);
#else /* not ANSI */
extern void argv ();
extern FILE *checked_fopen ();
extern boolean eoln ();
extern void fprintreal ();
extern integer inputint();
extern void fprintreal ();
extern char *xmalloc (), *xrealloc ();
extern integer zround ();
#endif /* not ANSI */

#endif /* not EXTRA_H */
