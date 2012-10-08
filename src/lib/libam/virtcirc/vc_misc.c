/*	@(#)vc_misc.c	1.5	94/04/07 10:21:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** vc_misc.c - virtual circuit miscellaneous module.
**
*/

#include <amtools.h>
#include <assert.h>
#include <semaphore.h>
#include <exception.h>
#include <circbuf.h>
#include <thread.h>
#include "vc.h"
#include "vc_int.h"

/*
** _vc_zap_cb - remove a circular buffer.
**
*/
void
_vc_zap_cb(vc, which)
	struct vc *vc;
	int which;
{
	assert(which == CLIENT || which == SERVER);

	_vc_my_p(1, "zap_cb: removing %s circular buffer\n", 
		(which == SERVER)? "server": "client");

	if (which == SERVER) {
		if (vc->vc_status & SERVER_CB_ALIVE) {
			vc->vc_status &= ~SERVER_CB_ALIVE;
			cb_free(vc->vc_cb[SERVER]);
		}
	}
	
	if (which == CLIENT) {
		if (vc->vc_status & CLIENT_CB_ALIVE) {
			vc->vc_status &= ~CLIENT_CB_ALIVE;
			cb_free(vc->vc_cb[CLIENT]);
		}
	}
}

/*
** die - clean up.
**
*/
void
_vc_die(vc, which)
	struct vc *vc;
	int which;
{
	_vc_my_p(1, "die: %s dying vc 0x%x.\n", 
		(which == SERVER)? "Server": "Client", vc);

	assert(which == SERVER || which == CLIENT);
	if (which == CLIENT) { 	/* Release waiting server thread */
		mu_unlock(&vc->vc_client_dead);
		if (vc->vc_warnf[CLIENT]) {
			_vc_my_p(2, "die: Calling client warning routine.\n");
			(vc->vc_warnf[CLIENT])(vc->vc_warnarg[CLIENT]);
		}
	}

	if (which == SERVER) {

		if (vc->vc_warnf[SERVER]) {
			_vc_my_p(2, "die: Calling server warning routine.\n");
			(vc->vc_warnf[SERVER])(vc->vc_warnarg[SERVER]);
		}

		_vc_my_p(2, "die: Waiting for client to die.\n");
		mu_lock(&vc->vc_client_dead);

		_vc_my_p(2, "die: Waiting for input to be closed.\n");
		mu_lock(&vc->vc_iclosed);

		_vc_my_p(2, "die: Removing client circular buffer.\n");
		_vc_zap_cb(vc, CLIENT);

		_vc_my_p(2, "die: Waiting for output to be closed.\n");
		mu_lock(&vc->vc_oclosed);

		_vc_my_p(2, "die: Removing server circular buffer.\n");
		_vc_zap_cb(vc, SERVER);

		mu_unlock(&vc->vc_done);

		/*
		** Now we're sure nobody will touch anything.
		**
		*/
		free(vc);
		_vc_my_p(1, "die: vc - done\n");
	}
	_vc_my_p(1, "die: %s is now dead.\n", 
		(which == SERVER)? "Server": "Client");
	thread_exit();
	/* NOTREACHED */
}


#ifdef __STDC__
#include <stdarg.h>
#define VA_START( ap, last ) va_start( ap, last )
#else
#include <varargs.h>
#define VA_START( ap, last ) va_start( ap )
#endif

#ifndef VC_DEBUG_LEVEL
#define VC_DEBUG_LEVEL	0
#endif

int32 _vc_debug_level = VC_DEBUG_LEVEL;
mutex print_mtx;

#ifdef __STDC__
void 
_vc_my_p(int32 level, ...)
#else
void
_vc_my_p(level, va_alist)
	int32 level;
	va_dcl
#endif
{
	static int got_debug_level = 0;
	va_list ap;

	if (!got_debug_level) {
		char *debug_level;

		if ((debug_level = getenv("VC_DEBUG_LEVEL")) != NULL) {
			_vc_debug_level = atoi(debug_level);
		} /* else don't override default */
		
		got_debug_level = 1;
	}

	if (level > _vc_debug_level)
		return;

	mu_lock(&print_mtx);
	VA_START(ap, level);
	{
		char *fmt = va_arg(ap, char *);

		printf("_vc_my_p: ");
		vprintf(fmt, ap);
		fflush(stdout);
	}
	va_end(ap);
	mu_unlock(&print_mtx);
}
