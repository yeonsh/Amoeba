/*	@(#)sigstuff.h	1.3	94/04/06 15:47:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __SIGSTUFF_H__
#define __SIGSTUFF_H__

#include <sys/types.h>
#include <signal.h> /* Posix */
#include <exception.h> /* Amoeba */
#include "sigsvr.h" /* Ajax */

extern sigset_t _ajax_sighandle; /* Signals with true handlers */
extern sigset_t _ajax_sigignore; /* Signals with SIG_IGN status */
extern sigset_t _ajax_sigblock;  /* Signals that are blocked */
extern sigset_t _ajax_sigpending;/* Signals that are pending (blocked) */

#define GOODSIG(sig) \
	((sig) > 0 && (sig) < NSIG && (_SIGMASK(sig) & _GOODSIGMASK) != 0)

typedef void (*handler)();

#define NILHANDLER ((handler)NULL)

#define EXCEPTION_SIGNALS \
	(_SIGMASK(SIGILL) | _SIGMASK(SIGBUS) | _SIGMASK(SIGTRAP) | \
	 _SIGMASK(SIGFPE) | _SIGMASK(SIGSEGV) | _SIGMASK(SIGSYS))

#define ISHANDLER(proc) ((proc) != SIG_IGN && (proc) != SIG_DFL)

#endif /* __SIGSTUFF_H__ */
