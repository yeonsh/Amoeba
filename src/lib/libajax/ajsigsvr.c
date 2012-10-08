/*	@(#)ajsigsvr.c	1.4	96/02/27 10:58:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* last modified apr 22 93 Ron Visser */
/* The signal server is a separate thread running in Ajax programs that
   have handlers for Unix signals.  It is started when the first Unix
   signal handler is installed.  Even though the signal vector is a
   per-thread property, there is only one signal server -- signals
   coming from the outside world are sent to all threads (potentially). */

#include "ajax.h"
#include "sesstuff.h"
#include "sigstuff.h"
#include "fdstuff.h"
#include "thread.h"

#undef TRACE
#define TRACE(s)	PUTS(s)
#undef TRACENUM
#define TRACENUM(s,n)	PUTNUM(s,n)

static int initlevel; /* Protected by _ajax_forkmu */
static port privport;
static capability sigsvrcap;

#ifndef SIG_HANDLER_BUG_FIXED
int _ajax_pending_sig = 0;		/* See assignment, below */
#endif


/*ARGSUSED*/
static void
sigsvr(p, s)
char *	p;
int	s;
{
	header h;
	short err;
	int sig;
	
	for (;;) {
		h.h_port = privport;
		if ((err = getreq(&h, NILBUF, 0)) != 0) {
			if (err == RPC_ABORTED) {
				TRACE("sigsvr: getreq interrupted by signal");
				(void) _ajax_startsigsvr(NILCAP);
				continue; /* After fork (?) */
			}
			TRACENUM("sigsvr: getreq err", (int) err);
			return;
		}
		if (h.h_command != AJAX_SENDSIG) {
			TRACENUM("sigsvr: got bad request", h.h_command);
			h.h_status = STD_COMBAD;
		}
		else if (!GOODSIG(h.h_extra)) {
			TRACENUM("sigsvr: got bad signal", (short)h.h_extra);
			h.h_status = STD_COMBAD;
		}
		else {
			sig = h.h_extra;
#ifndef SIG_HANDLER_BUG_FIXED
			/* BUG work around: Indicate to rest of the program
			 * that a signal has ocurred, and the user signal
			 * handler has not yet been called.  Sometimes the
			 * signal handler never gets called, so we want
			 * programs to be able to check this.  The var will
			 * be cleared by unixsignal(), when (if) the user
			 * handler is called.
			 */
			{	handler getsighandler();
				handler proc = getsighandler(sig);
				if (ISHANDLER(proc))
					/* There is a user signal handler: */
					_ajax_pending_sig = sig;
				TRACENUM("sigsvr: set _ajax_pending_sig", sig);
			}
#endif
			if (!sigismember(&_ajax_sigblock, sig)) {
				sig_raise(sig);
				TRACENUM("sigsvr: called sig_raise", sig);
			} else {
				sigaddset(&_ajax_sigpending, sig);
				TRACENUM("sigsvr: blocked signal", sig);
			}
			_ajax_deliver();
			h.h_offset = (_ajax_sigblock | _ajax_sighandle |
 				      _ajax_sigignore) & sigmask(sig);
			h.h_status = STD_OK;
		}
		(void) putrep(&h, NILBUF, 0);
		if (sig == SIGKILL) {
			TRACE("sigsvr: got SIGKILL");
			_ajax_call_exitprocs();
			/* Await process-signal from sessvr.
			   Block fdmu so other threads can't do I/O. */
			for (;;) {
				MU_LOCK(&_ajax_fdmu);
			}
		}
	}
}


int
_ajax_startsigsvr(cap)
	/*out*/ capability *cap;
{
	capability *session;
	long sigignore;
	
	SESINIT(session);
	MU_LOCK(&_ajax_forkmu);
	if (initlevel <= _ajax_forklevel) {
		uniqport(&privport);
		if (cap != NILCAP) {
			if (!thread_newthread(sigsvr, 2000, (char *)NULL, 0)) {
				MU_UNLOCK(&_ajax_forkmu);
				ERR(EIO, "startsigsvr: can't start thread");
			}
		}
		priv2pub(&privport, &sigsvrcap.cap_port);
		if (ses_sigaware(session, &sigsvrcap, &sigignore,
				 &_ajax_sigblock, &_ajax_sigpending) == 0)
			_ajax_sigignore |= sigignore;
		initlevel = _ajax_forklevel + 1;
	}
	MU_UNLOCK(&_ajax_forkmu);
	if (cap != NILCAP)
		*cap = sigsvrcap;
	return 0;
}
