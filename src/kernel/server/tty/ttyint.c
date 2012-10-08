/*	@(#)ttyint.c	1.7	96/02/27 14:21:46 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#if !VERY_SIMPLE

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "global.h"
#include "tty/term.h"
#include "assert.h"
INIT_ASSERT
#include "posix/termios.h"
#include "class/tios.h"
#include "posix/sys/types.h"
#include "posix/signal.h"
#include "sys/proto.h"

uint16 intsleeping;

void
ttyint()
{
	register struct io *iop;
	register char *p;
	register char *q;
#ifdef NOTIOS
	register c;
#endif
	register trans_done;
	header hdr;
	char buf[BMSIZE * 2];

	(void) timeout((interval) 3000);
	for (;;) {
		trans_done = 0;
		for (iop = &tty[0]; iop < &tty[ntty]; iop++)
			if (iop->io_state & (SINTED|SHANGUP|SINTRUP)) {
				if (NULLPORT(&iop->io_intcap.cap_port)) {
					iop->io_state &= ~(SINTED|SHANGUP|SINTRUP);
					continue;
				}
				p = buf;
				if (iop->io_state & SHANGUP) {
					hdr.h_command = TTI_HANGUP;
					iop->io_state &= ~SHANGUP;
				} else if (iop->io_state & SINTRUP) {
					hdr.h_command = TTI_INTRUP;
					iop->io_state &= ~SINTRUP;
				} else {
					iop->io_state &= ~SINTED;
#ifndef NOTIOS
					hdr.h_command = TTI_SIGNAL;
					/*
					** Determine signal number...
					**
					*/
					if (tstbit(iop->io_ints, 
						iop->io_intr_char))
						hdr.h_extra = SIGINT;
					else
					if (tstbit(iop->io_ints, 
						iop->io_quit_char))
						hdr.h_extra = SIGQUIT;
					else
					if (iop->io_susp_char != 
						((char) _POSIX_VDISABLE) &&
					    tstbit(iop->io_ints, 
						iop->io_susp_char))
						hdr.h_extra = SIGTSTP;
					else 
					/*
					** Unknown signal, programming error?
					** Send a SIGHUP.
					**
					*/
						hdr.h_extra = SIGHUP;
#else  /* !NOTIOS */
					hdr.h_command = TTI_INTERRUPT;
					iop->io_state &= ~SINTED;
					/* nice and simple, but not very efficient */
					for (c = 0; c < BMSIZE*16; c++)
						if (tstbit(iop->io_ints, c))
							*p++ = c;
#endif /* !NOTIOS */
					for (q = &iop->io_ints[0];
					     q < &iop->io_ints[BMSIZE]; q++)
						*q = 0;
				}
				hdr.h_size = p - buf;
				hdr.h_port = iop->io_intcap.cap_port;
				hdr.h_priv = iop->io_intcap.cap_priv;
				switch ((short) trans(&hdr, (bufptr) buf, (unsigned) (p - buf), &hdr, NILBUF, 0)) {
				case RPC_FAILURE:
					printf("ttyint: network failure talking to '%p'\n", &hdr.h_port);
					break;
				case RPC_ABORTED:
					printf("ttyint: timeout talking to '%p'\n", &hdr.h_port);
					break;
				case RPC_BADADDRESS:
					printf("ttyint: bad address talking to '%p'\n", &hdr.h_port);
					break;
				case RPC_NOTFOUND:
					printf("ttyint: port '%p' not found\n", &hdr.h_port);
					break;
				case 0:
					if ((short) hdr.h_status == STD_CAPBAD)
						printf("ttyint: bad capability talking to '%p'\n", &hdr.h_port);
					break;
				}
				trans_done = 1;
			}
		if (!trans_done) {
			intsleeping = 1;
			/* we need to ignore interruptions to our await */
			while (await((event) &intsleeping, 0L) < 0)
				/* do nothing */;
			assert(!intsleeping);
		}
	}
}

tty_intcap(iop, hdr, buf, n)
struct io *iop;
header *hdr;
char *buf;
{
	if (n != CAPSIZE) {
		hdr->h_status = STD_COMBAD;
		return 0;
	}
	iop->io_intcap = * (capability *) buf;
	hdr->h_status = STD_OK;
	hdr->h_size = 0;
	return 0;
}

#endif /* !VERY_SIMPLE */
