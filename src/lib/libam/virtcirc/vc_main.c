/*	@(#)vc_main.c	1.6	94/04/07 10:21:01 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** vc_main.c - virtual circuit main module.
**
*/

#include <amtools.h>
#include <assert.h>
#include <semaphore.h>
#include <exception.h>
#include <circbuf.h>
#include <thread.h>
#include <module/signals.h>
#include "vc.h"
#include "vc_int.h"

/* Don't wait longer than MAX_COMPLETION msec for the vc to be cleaned up */
#define MAX_COMPLETION	((interval) (60*1000))

/*
** vc_create - create a full-duplex virtual circuit.
**
*/
struct vc *
vc_create(iport, oport, isize, osize)
	port *iport, *oport;
	int isize, osize;
{
	struct vc *vc, **vcp[2];

	if (!memcmp(iport->_portbytes, oport->_portbytes,
		sizeof(iport->_portbytes)))
	{
		/* old tcpsvr compatibility stuff no longer supported */
		return(0);
	}
	
	/*
	** Allocate vc administration memory.
	**
	*/
	if ((vcp[SERVER] = (struct vc **)malloc(sizeof(struct vc *))) == 0)
		return(0);

	if ((vcp[CLIENT] = (struct vc **)malloc(sizeof(struct vc *))) == 0) {
		free(vcp[SERVER]);
		return(0);
	}

	if ((vc = (struct vc *)malloc(sizeof(struct vc))) == 0) {
		free(vcp[SERVER]);
		free(vcp[CLIENT]);
		return(0);
	}
	(void) memset((_VOIDSTAR) vc, 0, sizeof(struct vc));

	*vcp[SERVER] = *vcp[CLIENT] = vc;
	vc->vc_port[CLIENT] = *iport;
	vc->vc_port[SERVER] = *oport;

	/*
	** Allocate circular buffers.
	**
	*/
	if ((vc->vc_cb[CLIENT] = cb_alloc(isize)) == 0) {
		free(vc);
		free(vcp[SERVER]);
		free(vcp[CLIENT]);
		return(0);
	}

	if ((vc->vc_cb[SERVER] = cb_alloc(osize)) == 0) {
		cb_free(vc->vc_cb[CLIENT]);
		free(vc);
		free(vcp[SERVER]);
		free(vcp[CLIENT]);
		return(0);
	}
	_vc_my_p(2, "vc_create: circular buffers allocated.\n");

	/*
	** Initialize mutexes.
	**
	*/
	mu_init(&vc->vc_mutex);
	mu_init(&vc->vc_client_hup);
	mu_lock(&vc->vc_client_hup);
	mu_init(&vc->vc_client_dead);
	mu_lock(&vc->vc_client_dead);
	mu_init(&vc->vc_iclosed);
	mu_lock(&vc->vc_iclosed);
	mu_init(&vc->vc_oclosed);
	mu_lock(&vc->vc_oclosed);
	mu_init(&vc->vc_done);
	mu_lock(&vc->vc_done);

	vc->vc_status |= CLIENT_CB_ALIVE|SERVER_CB_ALIVE;

	vc->vc_sigs[SERVER] = sig_uniq();	/* Server to client signal */
	vc->vc_sigs[CLIENT] = sig_uniq();	/* Client to server signal */

	/*
	** Start server and client.
	**
	*/
	_vc_my_p(2, "vc_create: starting server.\n");
	if (thread_newthread(_vc_server, STACKSIZE, 
		 	  (char *)vcp[SERVER], sizeof(*vcp[SERVER])) == 0) {
		cb_free(vc->vc_cb[CLIENT]);
		cb_free(vc->vc_cb[SERVER]);
		free(vc);
		free(vcp[SERVER]);
		free(vcp[CLIENT]);
		return(0);
	}

	_vc_my_p(2, "vc_create: starting client.\n");
	if (thread_newthread(_vc_client, STACKSIZE, 
			  (char *)vcp[CLIENT], sizeof(*vcp[CLIENT])) == 0) {
		vc_close(vc, VC_BOTH);
		mu_unlock(&vc->vc_client_dead);
		vc->vc_status |= GOT_CLIENT_SIG;
		sig_raise(vc->vc_sigs[CLIENT]);
		free(vcp[CLIENT]);
		return(0);
	}
	return(vc);
}

void
_vc_close_client(vc)
	struct vc *vc;
{
	if ((vc->vc_status & ICLOSED) == 0) {
		vc->vc_status |= ICLOSED;
		_vc_my_p(1, "closing client circbuf\n");
		cb_close(vc->vc_cb[CLIENT]);
		mu_unlock(&vc->vc_iclosed);	/* Input closed now */
	}
}

void
_vc_close_server(vc)
	struct vc *vc;
{
	if ((vc->vc_status & OCLOSED) == 0) {
		vc->vc_status |= OCLOSED;
		_vc_my_p(1, "closing server circbuf\n");
		cb_close(vc->vc_cb[SERVER]);
		mu_unlock(&vc->vc_oclosed);	/* Output closed now */
	}
}

/*
** vc_close - Close one or both circular buffers.
**
*/
void
vc_close(vc, which)
	struct vc *vc;
	int which;
{
	int async = which & VC_ASYNC;

	which &= ~VC_ASYNC;
	_vc_my_p(1, "vc_close: closing %s\n",
		(which == VC_BOTH)? "both channels":
		(which == VC_IN)? "input channel": "output channel");

	if (which & VC_IN)
		_vc_close_client(vc);

	if (which & VC_OUT)
		_vc_close_server(vc);

	/*
	** Wait for completion.
	**
	*/
	if (!async && (vc->vc_status & ICLOSED) && (vc->vc_status & OCLOSED))
		(void) mu_trylock(&vc->vc_done, MAX_COMPLETION);
}

/*
** vc_read - read from vc.
**
*/
int
vc_read(vc, buf, size)
	struct vc *vc;
	bufptr     buf;
	int        size;
{
	_vc_my_p(2, "vc_read: %d bytes\n", size);
	return(cb_gets(vc->vc_cb[CLIENT], buf, 1, size));
}

/*
** vc_readall - read from vc.
**
*/
int
vc_readall(vc, buf, size)
	struct vc *vc;
	bufptr     buf;
	int        size;
{
	_vc_my_p(2, "vc_readall: %d bytes\n", size);
	return(cb_gets(vc->vc_cb[CLIENT], buf, size, size));
}

/*
** vc_write - write to a vc.
*/
int
vc_write(vc, buf, len)
	struct vc *vc;
	bufptr     buf;
	int        len;
{
	_vc_my_p(2, "vc_write: %d bytes\n", len);
	if (cb_puts(vc->vc_cb[SERVER], buf, len) < 0)
		return(-1);
    	return len;
}

/*
** vc_getp - get a circular buffer pointer to fetch data from.
**
*/
int
vc_getp(vc, buf, blocking)
	struct vc *vc;
	bufptr    *buf;
	int        blocking;
{
	return(cb_getp(vc->vc_cb[CLIENT], buf, blocking));
}

/*
** vc_getpdone - mark it done.
**
*/
void
vc_getpdone(vc, size)
	struct vc *vc;
	int        size;
{
	cb_getpdone(vc->vc_cb[CLIENT], size);
}

/*
** vc_putp - get a circular buffer pointer to store data in.
**
*/
int
vc_putp(vc, buf, blocking)
	struct vc *vc;
	bufptr    *buf;
	int        blocking;
{
	return(cb_putp(vc->vc_cb[SERVER], buf, blocking));
}
	
/*
** vc_putpdone - mark it done.
**
*/
void
vc_putpdone(vc, size)
	struct vc *vc;
	int        size;
{
	cb_putpdone(vc->vc_cb[SERVER], size);
}

/*
** vc_warn - Set a warning routine
**
*/
void
vc_warn(vc, which, rtn, arg)
	struct vc *vc;
	int which;
	void (*rtn) _ARGS(( int arg ));
	int arg;
{
	assert(which == VC_IN || which == VC_OUT);
	_vc_my_p(2, "vc_warn: %s\n", (which == VC_IN)? "VC_IN": "VCOUT");
	vc->vc_warnf[(which == VC_IN)? CLIENT: SERVER] = rtn;
	vc->vc_warnarg[(which == VC_IN)? CLIENT: SERVER] = arg;
}

/*
** vc_avail - See how many bytes (or buffer space) is available
*/
int
vc_avail(vc, which)
	struct vc *vc;
	int which;
{
	int avail;

	if (which != VC_IN && which != VC_OUT)
		return(-1);

    	if (which == VC_IN)
		avail = cb_full(vc->vc_cb[CLIENT]);
    	else
		avail = cb_empty(vc->vc_cb[SERVER]);
	_vc_my_p(2, "vc_avail: %s returning %d\n", 
		(which == VC_IN)? "Client": "Server", avail);
	return(avail);
}

/*
** vc_setsema - Set input semaphore.
**
*/
void
vc_setsema(vc, sema)
	struct vc *vc;
	semaphore *sema;
{
	_vc_my_p(2, "vc_setsema: setting input semaphore.\n");
	cb_setsema(vc->vc_cb[CLIENT], sema);
}
