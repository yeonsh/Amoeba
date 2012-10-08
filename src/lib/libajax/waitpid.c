/*	@(#)waitpid.c	1.6	96/02/27 10:59:45 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* waitpid() POSIX 3.2.1
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "sesstuff.h"
#include "sigstuff.h"
#include "thread.h"

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#define _LEGAL_OPT_MASK 	(WNOHANG | WUNTRACED)

static int initlevel; /* Protected by _ajax_forkmu */
static port callbackprivport;
static capability callback;

static int blockingwait();

pid_t
waitpid(pidarg, psts, options)
	pid_t pidarg;
	int *psts;
	int options;
{
	int pid = pidarg;
	int cause;
	long detail;
	capability *session;
	static capability nocallback; /* Remains zero */
	int err;
	
	if((options & ~_LEGAL_OPT_MASK) != 0)
		ERR(EINVAL, "waitpid: illegal options");

	SESINIT(session);
 restart:
	MU_LOCK(&_ajax_forkmu);
	if (initlevel <= _ajax_forklevel) {
		/* Don't generate a unique port every time, since this
		   will only fill the port cache and waste time in
		   one_way (when priv2pub calls it).
		   However, after a fork, we must reinitialize, or
		   parent and child would be using the same port! */
		uniqport(&callbackprivport);
		priv2pub(&callbackprivport, &callback.cap_port);
		initlevel = _ajax_forklevel + 1;
	}
	MU_UNLOCK(&_ajax_forkmu);
	if (ses_waitpid(session, /*in out*/ &pid, &cause, &detail,
			options&WNOHANG ? &nocallback : &callback) < 0)
		ERR(EIO, "waitpid: ses_waitpid failed");
	pid = (short)pid; /* XXX compensate bug in AIL stub. --Guido */
	if (pid == 0) {
		if (options&WNOHANG)
			return 0;
		err = blockingwait(&pid, &cause, &detail);
		if (err < 0)
			ERR(EIO, "waitpid: blockingwait failed");
		if (err > 0) {
			if (_ajax_sigignore & sigmask(err)) {
				TRACE("waitpid: restart after ignored signal");
				goto restart; /* After ignored signal */
			}
#ifdef NOUSIG
			sig_raise(err);
#endif
			ERR(EINTR, "waitpid: interrupted by signal");
		}
	}
	if (pid < 0)
		ERR(ECHILD, "waitpid: no children");
		TRACENUM("waitpid: cause", cause);
	if (psts != NULL) {
		switch (cause & ~0x80) {
		case TERM_NORMAL:
			*psts = (detail & 0xff) << 8;
			break;
		case TERM_STUNNED:
			if (detail <= 0 || detail >= NSIG)
				detail = SIGKILL;
			*psts = detail | (cause & 0x80);
			break;
		case TERM_EXCEPTION:
			*psts = _ajax_mapexception(detail) | (cause & 0x80);
			break;
		}
		TRACENUM("waitpid: sts", *psts);
	}
	TRACENUM("waitpid: return", pid);
	return pid;
}


/* The blocking wait uses a getreq() which will eventually receive a
   request from the session server.  We do this in a separate thread so
   it is possible to call waitpid() in a server between getreq() and
   putrep() -- yes, there are servers that use this!  Because a process
   can be waiting for only one child at a time (the session server
   doesn't support more than one wait) we can use global variables
   to pass things around.  A mutex is used so the main thread can wait
   for completion of the waiting thread. */

static int g_pid, g_cause;
static long g_detail;
static mutex g_block;
static int g_err;

static int
do_blockingwait(ppid, pcause, pdetail)
	int *ppid;
	int *pcause;
	long *pdetail;
{
	header  h;
	bufsize siz;
	errstat err;
	int     ret;
	
	for (;;) {
		h.h_port = callbackprivport;
		siz = getreq(&h, NILBUF, (bufsize) 0);
		if (ERR_STATUS(siz)) {
			if ((err = ERR_CONVERT(siz)) != RPC_ABORTED) {
				TRACENUM("waitpid.blockingwait: getreq error",
					 (int) err);
				return err;
			} /* else retry */
		}
		else break;
	}
	switch (h.h_command) {
	
	case PS_CHECKPOINT:
		*ppid = prv_number(&h.h_priv);
		TRACENUM("waitpid.blockingwait: got checkpoint, pid", *ppid);
		*pcause = h.h_extra;
		*pdetail = h.h_offset;
		ret = 0;
		h.h_status = STD_OK;
		break;
	
	case AJAX_SENDSIG:
		TRACENUM("waitpid.blockingwait: got signal", h.h_extra);
		ret = h.h_extra;
		h.h_status = STD_OK;
		break;
	
	default:
		TRACENUM("wait.blockingwait: unknown command", h.h_command);
		h.h_status = STD_COMBAD;
		ret = -1;
		break;
	
	}
	(void) putrep(&h, NILBUF, 0);
	return ret;
}

/*ARGSUSED*/
static void
call_blockingwait(param, size)
char *	param;
int	size;
{
	g_err = do_blockingwait(&g_pid, &g_cause, &g_detail);
	mu_unlock(&g_block);
}

static int
blockingwait(ppid, pcause, pdetail)
	int *ppid;
	int *pcause;
	long *pdetail;
{
	mu_init(&g_block);
	mu_lock(&g_block);
	if (!thread_newthread(call_blockingwait, 1000, (char *)NULL, 0))
		return STD_SYSERR;
	mu_lock(&g_block); /* Wait for completion of do_blockingwait() */
	if (g_err == 0) {
		*ppid = g_pid;
		*pcause = g_cause;
		*pdetail = g_detail;
	}
	return g_err;
}
