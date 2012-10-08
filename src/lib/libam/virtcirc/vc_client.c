/*	@(#)vc_client.c	1.9	96/02/27 11:04:46 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** vc_client.c - virtual circuit client module.
**
*/

#include <stdlib.h>
#include <amtools.h>
#include <assert.h>
#include <exception.h>
#include <semaphore.h>
#include <circbuf.h>
#include <module/signals.h>
#include "vc.h"
#include "vc_int.h"
#include "thread.h"


static void _vc_external_failure _ARGS(( struct vc *vc ));
static void _vc_client_hup_state _ARGS(( struct vc *vc, header *hdr ));

/*
** catcher for client signals.
*/
/*ARGSUSED*/
static void
_vc_client_catcher(signo, us, extra)
	signum	   signo;
	thread_ustate *us;
	_VOIDSTAR extra;
{
	struct vc *vc= (struct vc *)extra;

	if (signo == vc->vc_sigs[SERVER]) {
		_vc_my_p(1, "_vc_client_catcher: Got SIG_SERVER.\n");
		if (vc->vc_status & GOT_SERVER_SIG)
			_vc_external_failure(vc);
	}
	else {
		_vc_my_p(1, "_vc_client_catcher: got illegal signal %d\n", 
			signo);
		_vc_external_failure(vc);
		/* NOTREACHED */
	}
}

/*
** _vc_client - Code for client (reader) thread.
**
*/
/*ARGSUSED*/
void 
_vc_client(arg, argsize)
char *arg;
int argsize;
{
	struct vc **vcp= (struct vc **)arg;

	struct vc *vc = *vcp;
	header  hdr;
	bufsize rv;
	bufptr	rep_buf;
	bufsize rep_size;
	int     needwarn;

	_vc_my_p(1, "_vc_client: started (vc 0x%x).\n", vc);
	/*
	** Catch server signals and set locate timer.
	**
	*/
	(void)sig_catch(vc->vc_sigs[SERVER], _vc_client_catcher,(_VOIDSTAR)vc);
	(void)timeout((interval)30000);

	hdr.h_port = vc->vc_port[CLIENT];

	while (1) {
		/*
		** Get an amount of free bytes.
		**
		*/
		_vc_my_p(2, "_vc_client: calling putp\n");
		rep_size = cb_putp(vc->vc_cb[CLIENT], (char **)&rep_buf, -1);

		/* HUP, output cb closed */
		if ((short)rep_size <= 0) {
			_vc_my_p(1, "vc_client: cb_putp EOF.\n");
			_vc_zap_cb(vc, CLIENT);
			_vc_client_hup_state(vc, &hdr);
			/* NOTREACHED */
		}
		if (rep_size > MAX_TRANS) rep_size = MAX_TRANS;
		
		/*
		** We've got a receive buffer, request some bytes.
		**
		*/
		while (1) {
			/* Try to avoid hanging a long time in the transaction
			 * in case the connection is just being closed.
			 * Especially useful for X clients.
			 * I'm not sure it's safe to forget about the
			 * transaction altogether in this case.
			 */
			threadswitch();
			if (vc->vc_status & ICLOSED) {
				_vc_my_p(1, "_vc_client: ICLOSED\n");
				(void)timeout((interval)1000);
			}

			_vc_my_p(2, "vc_client: requesting DATA.\n");
			hdr.h_port = vc->vc_port[CLIENT]; /* (re-)init port */
			hdr.h_command = VC_DATA;
			hdr.h_size    = rep_size;
			rv = trans(&hdr, NILBUF, 0, &hdr, rep_buf, rep_size);

			_vc_my_p(2, "_vc_client: trans returned %d\n", rv);
			if (ERR_STATUS(rv) && (ERR_CONVERT(rv) != RPC_ABORTED))
			{
#ifdef VC_DEBUG
				if (ERR_CONVERT(rv) == RPC_FAILURE && 
				    getenv("VC_DEBUG"))
					abort();
#endif
				_vc_external_failure(vc);
			}

			if (ERR_CONVERT(rv) == RPC_ABORTED) {
				_vc_my_p(1, "vc_client: Got RPC_ABORTED.\n");
				continue;
			}

			if (hdr.h_status == VC_INTR) {
				_vc_my_p(1, "vc_client: Got VC_INTR.\n");
				continue;
			}

			if (hdr.h_status == VC_EOF  ||
			    hdr.h_status == VC_DATA) 
				break;

			/* i.e. VC_EOF_HUP */
			_vc_external_failure(vc);
			/* NOTREACHED */
		}

		/* received an eof. */
		if (hdr.h_status == VC_EOF) {
			_vc_my_p(1, "vc_client: EOF.\n");
			_vc_client_hup_state(vc, &hdr);
		}
		assert(hdr.h_status == VC_DATA);

		/*
		** Received some data, handle it.
		**
		*/

		needwarn = vc->vc_warnf[CLIENT] && 
				cb_full(vc->vc_cb[CLIENT]) == 0;
		_vc_my_p(2, "_vc_client: calling putpdone %d\n", rv);
		assert(rv == hdr.h_size);
		cb_putpdone(vc->vc_cb[CLIENT], rv);
		
		if (needwarn) {
			_vc_my_p(2, "_vc_client: Calling warning routine.\n");
			(vc->vc_warnf[CLIENT])(vc->vc_warnarg[CLIENT]);
		}
	}
}

/*
** client_hup_state - transmit a hup.
**
*/
static void
_vc_client_hup_state(vc, hdr)
	struct vc *vc;
	header    *hdr;
{
	port saveport;
	bufsize rv;

	saveport = hdr->h_port;
	_vc_my_p(1, "client_hup_state: Got an HUP.\n");
	/*
	** Close client circular buffer; release blocked threads.
	**
	*/
	_vc_close_client(vc);
	mu_unlock(&vc->vc_client_hup);
	while (1) {
		hdr->h_port    = saveport; /* (re-)init port */
		hdr->h_size    = 0;
		hdr->h_command = VC_HUP;
		rv = trans(hdr, NILBUF, 0, hdr, NILBUF, 0);
		_vc_my_p(2, "client_hup_state: trans returned %d\n", rv);
	
		if (ERR_STATUS(rv) && ERR_STATUS(rv) != RPC_ABORTED)
		{
#ifdef VC_DEBUG
			if (ERR_CONVERT(rv) == RPC_FAILURE && 
			    getenv("VC_DEBUG"))
				abort();
#endif
			_vc_external_failure(vc);
		}

		if (hdr->h_status == VC_INTR)
			continue;

		if (hdr->h_status == VC_HUP)
			break;

		_vc_external_failure(vc);
		/* NOTREACHED */
	}

	assert(hdr->h_status == VC_HUP);

	/*
	** We're done.
	**
	*/
	_vc_die(vc, CLIENT);
	/* NOTREACHED */
}

/*
** external_failure - the remote server crashed, did something wrong
** or whatever; clean up the mess.
**
*/
static void
_vc_external_failure(vc)
	struct vc *vc;
{

	_vc_my_p(1, "external_failure: Remote server crashed.\n");

	/*
	** Close client circular buffer, when the remote server has crashed
	** wakeup thread which is reading from the client circular buffer.
	**
	*/
	_vc_close_client(vc);

	/*
	** Set the client to server bit and raise the client to server signal.
	**
	*/
	mu_lock(&vc->vc_mutex);
	if ((vc->vc_status & GOT_SERVER_SIG) == 0) {
		_vc_my_p(1, "external_failure: Sending SIG_CLIENT\n");
		threadswitch();	/* magic */
		vc->vc_status |= GOT_CLIENT_SIG;
		sig_raise(vc->vc_sigs[CLIENT]);
	}
	mu_unlock(&vc->vc_mutex);
	
	_vc_die(vc, CLIENT);
	/* NOTREACHED */
}

