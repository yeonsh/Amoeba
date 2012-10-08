/*	@(#)vc_server.c	1.7	94/04/07 10:21:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** vc_server.c - virtual circuit server module.
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

static void _vc_local_server_failure _ARGS(( struct vc *vc ));

/*
** catcher for server signals.
*/
/*ARGSUSED*/
static void
_vc_server_catcher(signo, us, extra)
	signum	   signo;
	thread_ustate *us;
	_VOIDSTAR  extra;
{
	struct vc *vc= (struct vc *)extra;

	if (signo == SIG_TRANS) {
		_vc_my_p(2, "vc_server_catcher: Got TRANS_SIG.\n");
		vc->vc_status |= GOT_TRANS_SIG;
	}
	else
	if (signo == vc->vc_sigs[CLIENT]) {
		_vc_my_p(2, "vc_server_catcher: Got SIG_CLIENT.\n");
		if (vc->vc_status & GOT_CLIENT_SIG)
			_vc_local_server_failure(vc);
	}
	else {
		_vc_my_p(2, "_vc_server_catcher: got illegal signal %d\n", 
			signo);
		_vc_local_server_failure(vc);
		/* NOTREACHED */
	}
}

/*
** _vc_server - Code for server (writer) thread.
**
*/
/*ARGSUSED*/
void
_vc_server(arg, argsize)
char *arg;
int argsize;
{
	struct vc **vcp= (struct vc **)arg;

	struct vc *vc = *vcp;
	bufsize rv;
	header  hdr;
	bufptr	rep_buf;
	bufsize rep_size;
	int     needwarn;

	_vc_my_p(1, "_vc_server: started (vc 0x%x).\n", vc);
	/*
	** Catch Amoeba transaction and client signals.
	**
	*/
	(void)sig_catch(SIG_TRANS, _vc_server_catcher, (_VOIDSTAR) vc);
	(void)sig_catch(vc->vc_sigs[CLIENT], _vc_server_catcher, (_VOIDSTAR)vc);
	

	while (1) {
		do {
			hdr.h_port = vc->vc_port[SERVER];
			rv = getreq(&hdr, NILBUF, 0);
		} while (ERR_STATUS(rv) == RPC_ABORTED);
		if (ERR_STATUS(rv))
			_vc_local_server_failure(vc);

		rep_buf = NILBUF;
		/*
		** Check states (interrupted or killed)
		**
		*/
		if (vc->vc_status & GOT_TRANS_SIG) {
			/* signal meant for remote client */
			_vc_my_p(1, "vc_server: got SIG_TRANS\n");
			hdr.h_status = VC_INTR;
			hdr.h_size   = 0;
			putrep(&hdr, NILBUF, 0);
			vc->vc_status &= ~GOT_TRANS_SIG;
			continue;
		}

		/*
		** Now handle the received request.
		**
		*/
		switch (hdr.h_command) {
		case	VC_HUP:	
			_vc_my_p(2, "vc_server: Got HUP request.\n");
			/*
			** Close server cb; blocked threads will be released.
			**
			*/
			_vc_close_server(vc);
			hdr.h_size = 0;
			while (mu_trylock(&vc->vc_client_hup, -1L) < 0) 
				if (vc->vc_status & GOT_TRANS_SIG)
					break;

			/*
			** We've been interrupted.
			**
			*/
			if (vc->vc_status & GOT_TRANS_SIG) {
				hdr.h_status = VC_INTR;
				putrep(&hdr, NILBUF, 0);
				vc->vc_status &= ~GOT_TRANS_SIG;
				continue;	/* Goto GetRequest */
			}

			hdr.h_status = VC_HUP;
			putrep(&hdr, NILBUF, 0);
			_vc_die(vc, SERVER);
			/* NOTREACHED */

		case	VC_DATA:
			/*
			** Get data pointer and size from circular buffer.
			**
			*/
			_vc_my_p(2, "vc_server: Got DATA request.\n");
			rep_size = cb_getp(vc->vc_cb[SERVER], 
					     (char **)&rep_buf, -1);

			if (vc->vc_status & GOT_TRANS_SIG) {
				hdr.h_status = VC_INTR;
				hdr.h_size   = 0;
				_vc_my_p(2, "_vc_server: sending VC_INTR.\n");
				putrep(&hdr, NILBUF, 0);
				vc->vc_status &= ~GOT_TRANS_SIG;
				continue;	/* Goto GetRequest */
			}

			if ((short)rep_size <= 0) {
				rep_size = 0;
				hdr.h_status = ((vc->vc_status & ICLOSED) && 
			    		        (vc->vc_status & OCLOSED))?
					       VC_EOF_HUP: VC_EOF;
			}
			else
				hdr.h_status = VC_DATA;
			if (rep_size > MAX_TRANS) 
				rep_size = MAX_TRANS;
			if (rep_size > hdr.h_size)
				rep_size = hdr.h_size;

			/*
			** transmit the data.
			**
			*/
			hdr.h_size = rep_size;
			putrep(&hdr, rep_buf, rep_size);
			if (hdr.h_status == VC_DATA) {
				needwarn = vc->vc_warnf[SERVER] && 
					   cb_empty(vc->vc_cb[SERVER]) == 0;
				assert(hdr.h_size == rep_size);
				cb_getpdone(vc->vc_cb[SERVER], hdr.h_size);

				if (needwarn) {
					_vc_my_p(2, "_vc_server: Calling warning routine.\n");
					(vc->vc_warnf[SERVER])
						(vc->vc_warnarg[SERVER]);
				}
			}
			else
			if (hdr.h_status == VC_EOF_HUP) /* I and O cb closed */
				_vc_local_server_failure(vc);		
			else /* EOF, input cb closed */
				_vc_zap_cb(vc, SERVER);
			continue;

		case	STD_INFO:
			rep_buf      = VIRTCIRC;
			hdr.h_size   = strlen(rep_buf);
			hdr.h_status = STD_OK;
			break;

		default:
			_vc_my_p(1, "vc_server: got illegal request (0x%x)\n",
				hdr.h_command);
			hdr.h_status = STD_COMBAD;
			hdr.h_size   = 0;
			break;
		}
		putrep(&hdr, rep_buf, hdr.h_size);
	}
}

/*
** _vc_local_server_failure, called upon fatal errors.
**
*/
static void
_vc_local_server_failure(vc)
	struct vc *vc;
{
	/*
	** set the status bit and raise the server to client signal.
	**
	*/
	_vc_my_p(1, "_vc_local_server_failure: vc 0x%x\n", vc);

	/*
	** Close server circular buffer, blocked threads which where doing
	** circular buffer actions are released.
	**
	*/
	_vc_close_server(vc);

	mu_lock(&vc->vc_mutex);
	if ((vc->vc_status & GOT_CLIENT_SIG) == 0) {
		_vc_my_p(2, "_vc_local_server_failure: sending SIG_SERVER\n");
		vc->vc_status |= GOT_SERVER_SIG;
		sig_raise(vc->vc_sigs[SERVER]);
	}
	mu_unlock(&vc->vc_mutex);

	_vc_die(vc, SERVER);
	/* NOTREACHED */
}
