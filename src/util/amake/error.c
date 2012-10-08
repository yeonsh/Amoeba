/*	@(#)error.c	1.3	94/04/07 14:49:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#if __STDC__
#include <stdarg.h>
#define VA_START(x,y) va_start(x,y)
#else
#include <varargs.h>
#define VA_START(x,y) va_start(x)
#endif
#include "global.h"
#include "standlib.h"
#include "idf.h"
#include "type.h"
#include "scope.h"
#include "lexan.h"
#include "main.h"
#include "statefile.h"
#include "docmd.h"
#include "execute.h"
#include "expr.h"
#include "error.h"

PUBLIC  int ErrorsOccurred = 0;
PRIVATE int BadResult = FALSE;
PRIVATE int LexicalErrors = 0;

#ifdef EM_MOD
/* Use the smaller & simpler print module. */
#include <print.h>
#define vfprintf	doprnt
#endif

PRIVATE void PrintLine P(( void ));
PRIVATE void ErrCheck  P(( void ));

PUBLIC void
NewLine()
{
    fprintf(stderr, "\n");
}

extern struct f_info file_info;

PRIVATE void
PrintLine()
{
    if (F_reading) {
	fprintf(stderr, "%s, line %d: ", file_info.f_file, file_info.f_line);
    }
}

PRIVATE void
ErrCheck()
/* Geef niet meer dan MAX_ERRORS fouten */
{
    if (++ErrorsOccurred > MAX_ERRORS) {
	FatalError("too many errors");
    }
}

PUBLIC void
SetBadExitStatus()
{
    BadResult = TRUE;
}

PUBLIC int
GetExitStatus()
{
    return((BadResult || ErrorsOccurred) ? EXIT_FAILURE : EXIT_SUCCESS);
}

PUBLIC struct idf *
ErrorId()
/* Inserteer een nieuwe error identifier "_err<num>" in de naamlijst. */
{
    static int errnum = 0;
    static char errstring[] = "_err0000";
    struct idf *idp;

    (void) sprintf(errstring, "_err%d", errnum++);
    (idp = str2idf(errstring, 1))->id_flags |= F_ERROR_ID;
    return(idp);
}

PUBLIC void
#if __STDC__
LexError(char *fmt, ...)
#else
/*VARARGS1*/
LexError(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    PrintLine(); vfprintf(stderr, fmt, args); NewLine();
    if (++LexicalErrors > MAX_LEX_ERRORS) {
	FatalError("too many lexical errors");
    }
    ErrCheck();
    va_end(args);
}

PUBLIC void
#if __STDC__
PrintError(char *fmt, ...)
#else
/*VARARGS1*/
PrintError(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

PUBLIC void
Exit()
{
    DumpStatefile();	/* keep the results we *have* got */
    exit(EXIT_FAILURE);
}

PUBLIC void
#if __STDC__
FatalError(char *fmt, ...)
#else
/*VARARGS1*/
FatalError(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    fprintf(stderr, "fatal error: ");
    vfprintf(stderr, fmt, args); NewLine();
    JobTrace();		/* producing a job trace is sometimes helpful */
    ErrorsOccurred++;	/* fact used in pwait() */
    AwaitProcesses();
    PrintError("Amake exits..\n");
    Exit();
    va_end(args);
}

PUBLIC void
#if __STDC__
ParseError(char *fmt, ...)
#else
/*VARARGS1*/
ParseError(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    PrintLine(); vfprintf(stderr, fmt, args); NewLine();
    ErrCheck();
    va_end(args);
}

PUBLIC void
#if __STDC__
SemanticError(char *fmt, ...)
#else
/*VARARGS1*/
SemanticError(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    PrintLine(); vfprintf(stderr, fmt, args); NewLine();
    ErrCheck();
    va_end(args);
}

PUBLIC void
#if __STDC__
RunTimeError(char *fmt, ...)
#else
/*VARARGS1*/
RunTimeError(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    vfprintf(stderr, fmt, args); NewLine();
    ErrCheck();
    va_end(args);
}

PUBLIC void
#if __STDC__
SemanticIdError(struct idf *idp, char *fmt, ...)
#else
/*VARARGS2*/
SemanticIdError(idp, fmt, va_alist)
struct idf *idp;
char *fmt;
va_dcl
#endif
/* Geef een melding als 'idp' geen error variabele is. 'idp' is mogelijk
 * niet gedeclareerd.
 */
{
    va_list args;

    VA_START(args, fmt);
    if ((idp->id_flags & F_ERROR_ID) == 0) {
	PrintLine();
	fprintf(stderr, "'%s': ", idp->id_text);
	vfprintf(stderr, fmt, args); NewLine();
    }
    ErrCheck();
    va_end(args);
}

PUBLIC void
#if __STDC__
Warning(char *fmt, ...)
#else
/*VARARGS1*/
Warning(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    if (!F_no_warnings) {
	if (F_reading) {
	    PrintLine();
	}
	fprintf(stderr, "(Warning) "); vfprintf(stderr, fmt, args); NewLine();
    }
    va_end(args);
}

PUBLIC void
#if __STDC__
Message(char *fmt, ...)
#else
/*VARARGS1*/
Message(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
    va_list args;

    VA_START(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

#define PRINT_LENGTH	1000
#define MARGIN		10

PUBLIC void
SafeMessage(long_string)
char *long_string;
{
    /* Watch out to overflow vfprintf's buffer */
    register int len = strlen(long_string);
    register char *p = long_string;

    while (len > PRINT_LENGTH) {
	char *finish = p + PRINT_LENGTH;
	int c = *finish;

	*finish = '\0'; 	/* terminate at position PRINT_LENGTH */
	Message("%s", p);
	*finish = c;		/* put it back */
	len -= PRINT_LENGTH;
	p = finish;
    }
    Message("%s", p);
}

PUBLIC void
#if __STDC__
Verbose(int level, char *fmt, ...)
#else
/*VARARGS1*/
Verbose(level, fmt, va_alist)
int level;
char *fmt;
va_dcl
#endif
{
    va_list args;

    if (F_verbose && level <= Verbosity) {
	VA_START(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
    }
}

PUBLIC void
#if __STDC__
FuncError(struct expr *call, char *fmt, ...)
#else
FuncError(call, fmt, va_alist)
struct expr *call; /* the '(' node */
char *fmt;
va_dcl
#endif
{
    va_list args;

    fprintf(stderr, "%s: ", call->e_func->id_text);
    VA_START(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    NewLine();
    ErrCheck();

    call->e_type = T_ERROR;	/* stop when call is evaluated */
}

PUBLIC void
CaseError(fmt, num)
char *fmt;
int num;
{
    PrintLine();
    fprintf(stderr, "Amake internal error: wrong case in switch, ");
    fprintf(stderr, fmt, num); NewLine();
    AwaitProcesses();
    DumpStatefile();	/* keep the results we *have* got */
    abort();
}
