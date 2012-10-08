/*	@(#)sigaction.c	1.3	94/04/07 09:51:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigaction() POSIX 3.3.4
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sigstuff.h"
#include "module/signals.h"

#include "fault.h"

#undef TRACE
#define TRACE(s)        PUTS(s)
#undef TRACENUM
#define TRACENUM(s,n)   PUTNUM(s,n)


#ifndef SIG_HANDLER_BUG_FIXED
extern int _ajax_pending_sig;	/* See assignment, below */
#endif

static struct sigaction ux_handler[NSIG];

static void
unixsignal(sig, fp, extra)
	signum sig;
	thread_ustate *fp;
	_VOIDSTAR extra;
{
	handler proc;
	sigset_t old_mask;
	
	TRACENUM("signal handler: got signal", sig);
	if (sig < 0) {
		sig = _ajax_mapexception(sig);
		TRACENUM("signal handler: exception mapped to signal", sig);
	}
	proc = ux_handler[sig].sa_handler;
	if (ISHANDLER(proc)) {
	    if(sigismember(&_ajax_sigblock,sig)){
		PUTNUM("unixsignal: add pending signal",sig);
		sigaddset(&_ajax_sigpending,sig);
	    } else {
		TRACE("signal handler: call unix handler");
#ifndef SIG_HANDLER_BUG_FIXED
		/* Indicate to rest of program that the signal handler has
		 * been called.  This variable is a bug workaround because
		 * signal handlers are often not called when they should be
		 * See also ajsigsvr.c.
		 */
		_ajax_pending_sig = 0;
#endif
		old_mask = _ajax_sigblock;
		_ajax_sigblock |= ux_handler[sig].sa_mask;
		sigdelset(&_ajax_sigblock,SIGKILL);
#ifdef _POSIX_JOB_CONTROL
		sigdelset(&_ajax_sigblock,SIGSTOP);
#endif
		sigaddset(&_ajax_sigblock,sig);

		sigdelset(&_ajax_sigpending,sig);
		(*proc)(sig, sig, fp);
		_ajax_deliver();
		_ajax_sigblock = old_mask;
	    }
	}
	TRACE("signal handler: returning");
}

int
_ajax_deliver()
{
/* delivers one or more pending unblocked signals, the number of
 * signals that is delivered is returned.
 */
  int sig,cnt = 0;

  for (sig = 1; sig < NSIG; ++sig)
	if (GOODSIG(sig))
		if (sigismember(&_ajax_sigpending,sig) )
			if (!sigismember(&_ajax_sigblock,sig))
			{
				PUTNUM("_ajax_deliver: signal",sig);
				unixsignal(sig,(thread_ustate *) 0,NULL);
				cnt++;
			}else PUTNUM("blocked",sig);
  return cnt;
}

int
sigaction(sig, act, oact)
	int sig;
	struct sigaction *act, *oact;
{
	struct sigaction prev;
	capability sigsvrcap;
	
	if (!GOODSIG(sig) || sig == SIGKILL)
		ERR(EINVAL, "sigaction: bad signal number");
	
	/* Start signal server (this also initializes 
	 * _ajax_sigignore, _ajax_sigpending and _ajax_sigblock) */
	if (_ajax_startsigsvr(&sigsvrcap) < 0)
		return -1;
	
	/* Save old Unix handler */
	/* (This is done here so the code works if act == oact) */
	prev = ux_handler[sig];
	if (sigismember(&_ajax_sigignore, sig))
		prev.sa_handler = SIG_IGN; /* If ignored at exec time */
	
	if (act != NULL) {
		/* Set new handler */
		handler proc = act->sa_handler;
		
		/* Set Unix handler */
		ux_handler[sig] = *act;
	
		/* Update ignore and handle masks */
		sigdelset(&_ajax_sigignore, sig);
		sigdelset(&_ajax_sighandle, sig);
		if (proc == SIG_IGN) {
			TRACENUM("sigaction: ignore signal", sig);
			sigaddset(&_ajax_sigignore, sig);
			sigdelset(&_ajax_sigpending, sig);
		}
		else if (proc == SIG_DFL) {
			TRACENUM("sigaction: default signal", sig);
		}
		else {
			TRACENUM("sigaction: handle signal", sig);
			sigaddset(&_ajax_sighandle, sig);
		}
	
		/* Set Amoeba handler */
		if (ISHANDLER(proc))
			(void) sig_catch(sig, unixsignal, (_VOIDSTAR) 0);
		else
			(void) sig_catch(sig, NILHANDLER, (_VOIDSTAR) 0);
	
		/* Set Amoeba handlers for exceptions mapped to this signal */
		if (sigmask(sig) & EXCEPTION_SIGNALS) {
			int exc;
			if (proc == SIG_DFL)
				proc = NILHANDLER;
			else
				proc = unixsignal;
			for (exc = EXC_ILL; exc >= EXC_LAST; --exc) {
				if (_ajax_mapexception(exc) == sig)
				    (void) sig_catch(exc, proc, (_VOIDSTAR) 0);
			}
		}
	}
	
	if (oact != NULL) { /* Report old Unix handler */
		*oact = prev;
	}
	
	return 0;
}

/*
 * Report current setting of handler for sig, without taking any other
 * action:
 */
handler getsighandler(sig)
int sig;
{
	return ux_handler[sig].sa_handler;
}


#ifndef SIG_HANDLER_BUG_FIXED
/* Sometimes the user's interrupt handler doesn't get called, so we make
 * available to user code the following function to check whether the bug
 * has occurred and, if so, call the user's handler.  It can be called as
 * often as desired, without damage.
 */
void sigcallpending()
{
	if (_ajax_pending_sig > 0) {
		handler proc = ux_handler[_ajax_pending_sig].sa_handler;
		_ajax_pending_sig = 0;
		TRACENUM("sigcallpending: sig handler bug; sighandler", proc);
		/* XXX this should call unixsignal, to set the new signal mask.
		 * If the signal is blocked it should not be called at all.
		 */
		if (ISHANDLER(proc))
			(*proc)();
	}
}
#endif
