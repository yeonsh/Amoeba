/*	@(#)tc_frombsd.c	1.3	94/04/07 11:13:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Convert a couple of BSD structures to a termios structure */

#ifdef SYSV

#ifndef lint

static int __NOT_USED;

#endif /* lint */

#else /* SYSV */

#include <sgtty.h>
#include "posix/termios.h"
#include <stdlib.h>
#include <string.h>

void
tc_frombsd(tp, sgp, tcp, ltcp, lmp)
	struct termios *tp;
	struct sgttyb *sgp;
	struct tchars *tcp;
	struct ltchars *ltcp;
	int *lmp;
{
	(void) memset(tp, '\0', sizeof(*tp));
	tp->c_cflag |= CREAD | CS8;
	
	/* In case ICANON gets turned off */
	
	tp->c_cc[VMIN] = 1;
	tp->c_cc[VTIME] = 0;
	
	/* From basic sgttyb */
	
	cfsetispeed(tp, sgp->sg_ispeed);
	cfsetospeed(tp, sgp->sg_ospeed);
	tp->c_cc[VERASE] = sgp->sg_erase;
	tp->c_cc[VKILL] = sgp->sg_kill;
	if (sgp->sg_flags & ECHO)
		tp->c_lflag |= ECHO;
	if (sgp->sg_flags & CRMOD) {
		tp->c_iflag |= ICRNL;
		if (!(*lmp & LLITOUT))
			tp->c_oflag |= OPOST;
	}
	if ((sgp->sg_flags & TBDELAY) == XTABS)
		tp->c_oflag |= XTAB;
	if (!(sgp->sg_flags & RAW)) {
		tp->c_lflag |= ISIG;
		if (!(sgp->sg_flags & CBREAK))
			tp->c_lflag |= ICANON;
	}
	
	/* From special chars */
	
	tp->c_cc[VINTR] = tcp->t_intrc;
	tp->c_cc[VQUIT] = tcp->t_quitc;
	tp->c_cc[VSTART] = tcp->t_startc;
	tp->c_cc[VSTOP] = tcp->t_stopc;
	tp->c_cc[VEOF] = tcp->t_eofc;
	tp->c_cc[VEOL] = tcp->t_brkc;
	
	/* From local special chars */
	
	tp->c_cc[VSUSP] = ltcp->t_suspc;
	
	/* From local mode */
	
	if (*lmp & LCRTERA)
		tp->c_lflag |= ECHOE;
	/* LITOUT is checked above */
	if (*lmp & LTOSTOP)
		tp->c_lflag |= TOSTOP;
	if (*lmp & LCRTKIL)
		tp->c_lflag |= ECHOK;
	if (*lmp & LNOFLSH)
		tp->c_lflag |= NOFLSH;

}
#endif /* not SYSV */
