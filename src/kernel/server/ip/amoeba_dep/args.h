/*	@(#)args.h	1.1	91/11/25 12:57:47 */
/* The <args.h> header checks whether the compiler claims conformance to ANSI
 * Standard C. If so, the symbol ANSI is defined as 1, otherwise it is 
 * defined as 0.  Based on the result, a macro
 *
 *	ARGS(( params ))
 *
 * is defined.  This macro expands in different ways, generating either
 * ANSI Standard C prototypes or old-style K&R (Kernighan & Ritchie) 
 * prototypes, as needed.  Finally, some programs use CONST, VOIDSTAR etc
 * in such a way that they are portable over both ANSI and K&R compilers.
 * The appropriate macros are defined here.
 */

#ifndef ARGS_H
#define ARGS_H

/* ANSI C requires __STDC__ to be defined as 1 for an ANSI C compiler.
 * Some half-ANSI compilers define it as 0.  Get around this here.
 */

#define ANSI              0	/* 0 if compiler is not ANSI C, 1 if it is */

#ifdef __STDC__			/* __STDC__ defined for (near) ANSI compilers*/
#if __STDC__ == 1		/* __STDC__ == 1 for conformant compilers */
#undef ANSI			/* get rid of above definition */
#define ANSI              1	/* _ANSI = 1 for ANSI C compilers */
#endif
#endif

/* At this point, ANSI has been set correctly to 0 or 1. Define the
 * ARGS macro to either expand both of its arguments (ANSI prototypes),
 * only the function name (K&R prototypes).
 */

#if ANSI
#define	ARGS(params)	params
#define	VOIDSTAR	void *
#define	VOID		void
#define	CONST		const
#define	VOLATILE	volatile
#define SIZET		size_t

#else

#define	ARGS(params)	()
#define	VOIDSTAR	char *
#define	VOID		int
#define	CONST
#define	VOLATILE
#define SIZET		int

#endif /* ANSI */

#undef ANSI

#endif /* ARGS_H */
