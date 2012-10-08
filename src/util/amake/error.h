/*	@(#)error.h	1.3	94/04/07 14:49:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define MAX_ERRORS	200
#define MAX_LEX_ERRORS	20

#if __STDC__
#define PROTO(x) x
#else
#define PROTO(x) ()
#endif

EXTERN int ErrorsOccurred;

PUBLIC void SetBadExitStatus	PROTO(( void ));
PUBLIC int GetExitStatus	PROTO(( void ));
PUBLIC void Exit		PROTO(( void ));

/*
 * Number of times an error-reporting function has been called
 */

EXTERN void LexError		PROTO(( char *format, ... ));
EXTERN void FatalError		PROTO(( char *format, ... ));
EXTERN void ParseError		PROTO(( char *format, ... ));
EXTERN void RunTimeError	PROTO(( char *format, ... ));
EXTERN void SemanticError	PROTO(( char *format, ... ));
EXTERN void Verbose		PROTO(( int level, char *format, ... ));
/*
 * When more than MAX_LEX_ERRORS lexical errors are reported a
 * FatalError terminates the program, because presumably a non-Amake
 * source file is being read.
 * The same happens when more than MAX_ERRORS errors are reported.
 */

EXTERN void SemanticIdError	PROTO(( struct idf *idp, char *fmt, ... ));
/* Write a message iff idp is not a generated error-identifier */
EXTERN void Message		PROTO(( char *format, ... ));
EXTERN void SafeMessage		PROTO(( char *long_string ));
EXTERN void PrintError		PROTO(( char *format, ... ));
EXTERN void Warning		PROTO(( char *format, ... ));
EXTERN void FuncError		PROTO(( struct expr *call, char *fmt, ... ));

/* Write a newline on stderr */
EXTERN void NewLine		PROTO(( void ));

EXTERN struct idf *ErrorId	PROTO(( void ));
/* Create new identifier and deliver a pointer to it. The name is of
 * the form _err<n>, so this shouldn't conflict with user-supplied names.
 */

EXTERN void CaseError		PROTO(( char *format, int num ));
/* Call when default case in switch shouldn't be reached */

#undef PROTO
