/*	@(#)ttq_intcap.c	1.3	94/04/07 09:54:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <tty/tty.h>

int
ttq_intcap(tty, handler)
	capability *tty;
	capability *handler;
{
	header h;
	short err;
	
	h.h_port = tty->cap_port;
	h.h_priv = tty->cap_priv;
	h.h_command = TTQ_INTCAP;
	if ((err = trans(&h, (char *) handler, CAPSIZE, &h, NILBUF, 0)) == 0)
		err =  h.h_status;
	return err;
}
