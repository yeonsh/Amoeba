/*	@(#)config.h	1.2	96/02/27 13:01:41 */
/*
 * Configuration file for the PD ksh
 *
 * RCSid: $Id: config.h,v 1.2 1994/11/14 09:06:59 versto Exp $
 */

#ifndef	_CONFIG_H
#define	_CONFIG_H

/*
 * Builtin edit modes
 */

#define	EMACS				/* EMACS-like mode */
#define	VI				/* vi-like mode */
#ifndef AMOEBA
#define	JOBS				/* job control */
#endif

#ifndef SIGINT
#include <signal.h>
#endif

/*
 * leave USE_SIGACT defined.
 * if you don't have sigaction(2) and the
 * implementation in sigact.c doesn't work for your system,
 * fix it.
 * 
 * Of course if your system has a real sigaction() 
 * implementation that is faulty! undef JOBS and add USE_SIGNAL
 * or whatever does work.  You may find it necessary to undef
 * USE_SIGACT, if so please report it.
 */
#define USE_SIGACT			/* POSIX signal handling */
/* 
 * These control how sigact.c implements sigaction()
 * If you don't define any of them it will try and work it out 
 * for itself.  The are listed in order of preference (usefulness).
 */
/* #define USE_SIGMASK			/* BSD4.2 ? signal handling */
/* #define USE_SIGSET			/* BSD4.1 ? signal handling */
/* #define USE_SIGNAL			/* plain old signal(2) */

#if defined(JOBS) && (!defined(SIGCONT) || (defined(_SYSV) && defined(USE_SIGNAL)))
#undef JOBS
#endif

/* #define	FASCIST			/* Fascist getopts */
#ifndef AMOEBA
#define	SHARPBANG			/* Hack to handle #! */
#endif
/* #define	SILLY			/* Game of life in EMACS mode */
/* #define	SWTCH			/* Handle SWTCH for shl(1) */


/*
 * ALTERNATIONS is csh not ksh, but it is such a nice feature...
 */
#define ALTERNATIONS			/* csh {a,b,c} arg expansion */

/* #define COMPLEX_HISTORY			/* Peter Collinson's history */
/*
 * if you don't have mmap() you can't use Peter Collinson's history
 * mechanism.  If that is the case, then define EASY_HISTORY
 */
#if !defined(COMPLEX_HISTORY) || defined(NO_MMAP) || defined(AMOEBA)
# define EASY_HISTORY			/* sjg's trivial history file */
#endif
  
#endif	/* _CONFIG_H */
