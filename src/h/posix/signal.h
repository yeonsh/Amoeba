/*	@(#)signal.h	1.5	94/04/06 16:54:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix 1003.1 compliant signal interface */

#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include "_ARGS.h"

/* 3.3 Signals */

/* 3.3.1.1 Signal Names */

/* Table 3.1 Required signals */

#define SIGABRT		6		/* ~v7 SIOGIOT */
#define SIGALRM		14
#define SIGFPE		8
#define SIGHUP		1
#define SIGILL		4
#define SIGINT		2
#define SIGKILL		9
#define SIGPIPE		13
#define SIGQUIT		3
#define SIGSEGV		11
#define SIGTERM		15
#define SIGUSR1		30
#define SIGUSR2		31

/* Table 3.2 Job Control Signals (not supported but must still be defined) */

#define SIGCHLD		20
#define SIGCONT		19
#define SIGSTOP		17
#define SIGTSTP		18
#define SIGTTIN		21
#define SIGTTOU		22


/* 3.3.2 kill(). */

int kill _ARGS((int _pid, int _sig));
	/* XXX The type of pid is really pid_t from <sys/types.h>, but we
	   can't use that typedef here since we can't require the user
	   to include <sys/types.h> and we can't export pid_t either. */

/* 3.3.3 Manipulate Signal Sets. */

typedef long sigset_t;

#define _SIGMASK(signo)			( 1 << ((signo)-1) )

#define sigemptyset(pset)		(*(pset) = 0, 0)
#define sigfillset(pset)		(*(pset) = _GOODSIGMASK, 0)

int sigaddset _ARGS((sigset_t *_pset, int _signo));
int sigdelset _ARGS((sigset_t *_pset, int _signo));
int sigismember _ARGS((sigset_t *_pset, int _signo));

/* 3.3.4 sigaction(). */

struct sigaction {
	void		(*sa_handler) _ARGS(( int _signo ));
	sigset_t	sa_mask;
	int		sa_flags;
};

/* Special values for sa_handler: */

#define SIG_DFL	  ( (void(*)()) 0 )	/* Request default signal handling */
#define SIG_ERR   ( (void(*)()) -1 )	/* Return from signal() upon error */
#define SIG_IGN	  ( (void(*)()) 1 )	/* Request that signal be ignored */

/* Value for sa_flags: */

#define SA_NOCLDSTOP	1

int sigaction _ARGS((int _sig, struct sigaction *_act,
		     struct sigaction *_oact));

/* 3.3.5 sigprocmask(). */

int sigprocmask _ARGS((int _how, sigset_t *_set, sigset_t *_oset));

/* Values for how: */

#define SIG_BLOCK		0
#define SIG_UNBLOCK		1
#define SIG_SETMASK		2

/* 3.3.6 sigpending(). */

int sigpending _ARGS((sigset_t *_set));

/* 3.3.7 sigsuspend(). */

int sigsuspend _ARGS((sigset_t *_sigmask));

/* ANSI C */

int raise _ARGS((int _sig));

void (*signal _ARGS((int _sig, void (*func)(int)))) _ARGS((int));

typedef int sig_atomic_t;


#ifdef AMOEBA

/* Extensions */

#define SIGAMOEBA	7	/* Amoeba client-to-server signal */

#define SIGSYS		12	/* Illegal instruction */
#define SIGTRAP		5	/* Trace trap */
#define SIGBUS		10	/* Bus error */

#define SIGEMT		SIGAMOEBA
#define SIGIOT		SIGABRT

#define NSIG		32

#define sigmask(signo) _SIGMASK(signo)

#endif /* AMOEBA */

/* Internal */

#define _GOODSIGMASK \
	(_SIGMASK(SIGABRT)	| \
	_SIGMASK(SIGALRM)	| \
	_SIGMASK(SIGFPE)	| \
	_SIGMASK(SIGHUP)	| \
	_SIGMASK(SIGILL)	| \
	_SIGMASK(SIGINT)	| \
	_SIGMASK(SIGKILL)	| \
	_SIGMASK(SIGPIPE)	| \
	_SIGMASK(SIGQUIT)	| \
	_SIGMASK(SIGSEGV)	| \
	_SIGMASK(SIGTERM)	| \
	_SIGMASK(SIGUSR1)	| \
	_SIGMASK(SIGUSR2)	| \
	_SIGMASK(SIGAMOEBA)	| \
	_SIGMASK(SIGSYS)	| \
	_SIGMASK(SIGTRAP)	| \
	_SIGMASK(SIGBUS))

#endif /* __SIGNAL_H__ */
