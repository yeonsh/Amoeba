/*	@(#)ajsesinit.c	1.3	94/04/07 09:42:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ajax.h"
#include "sesstuff.h"

static mutex mu;
int _ajax_sesinited;
capability _ajax_session;

int
_ajax_sesinit()
{
	capability owner;	/* Current owner; may be session svr private */
	capability self;	/* This process */
	capability *session;	/* Most specific session svr cap */
	capability *tty;	/* TTY (stdin) */
	int pid;		/* Process ID if known */
	int err;
	
	MU_LOCK(&mu);
	if (!_ajax_sesinited) {
		session = getcap("_SESSION");
		if (session == NULL) {
			MU_UNLOCK(&mu);
			_ajax_puts(
				"Ajax (init): can't find _SESSION in cap env");
			ERR(EIO, "sesinit: no _SESSION in cap env");
		}
		if (_ajax_getowner(&owner) != 0) {
			MU_UNLOCK(&mu);
			return -1; /* Can't find owner capability */
		}
		if (getinfo(&self, NILPD, 0) < 0) {
			MU_UNLOCK(&mu);
			ERR(EIO, "sesinit: getinfo failed");
		}
		if (PORTCMP(&owner.cap_port, &session->cap_port)) {
			/* Session svr already owns us */
			session = &owner;
			pid = prv_number(&owner.cap_priv);
		}
		else {
			/* Owner is not the session svr */
			pid = 0;
		}
		if ((err = ses_init(session, &self, &_ajax_session)) < 0) {
			MU_UNLOCK(&mu);
			_ajax_putnum("Ajax (init): session init failed, error",
									err);
			ERR(EIO, "sesinit: ses_init failed");
		}
		if (pid == 0) {
			/* Arrange for tty interrupts to go to the
			 * session server.
			 */
			if ((tty = getcap("TTY")) != NULL) {
				(void) ttq_intcap(tty, &_ajax_session);
			}
		}
		_ajax_sesinited = 1;
	}
	MU_UNLOCK(&mu);
	return 0;
}
