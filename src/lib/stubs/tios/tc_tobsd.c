/*	@(#)tc_tobsd.c	1.2	94/04/07 11:14:05 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Convert a termios structure to a couple of BSD structures.
   The BSD structures are supposed to be initialized with the current state,
   or at least zeroed out (this is not symmetric with tc_frombsd()!). */

#ifdef SYSV

#ifndef lint

static int __NOT_USED;

#endif /* lint */

#else /* SYSV */

#include <stdio.h>
#include <sgtty.h>
#include "posix/termios.h"

void
tc_tobsd(tp, sgp, tcp, ltcp, lmp)
	struct termios *tp;
	struct sgttyb *sgp;
	struct tchars *tcp;
	struct ltchars *ltcp;
	int *lmp;
{
	/* To basic sgttyb */
	
	sgp->sg_ispeed = cfgetispeed(tp);
	sgp->sg_ospeed = cfgetospeed(tp);
	sgp->sg_erase = tp->c_cc[VERASE];
	sgp->sg_kill = tp->c_cc[VKILL];
	
	if (tp->c_lflag & ECHO)
		sgp->sg_flags |= ECHO;
	else
		sgp->sg_flags &= ~ECHO;
	
	if (tp->c_iflag & ICRNL)
		sgp->sg_flags |= CRMOD;
	else
		sgp->sg_flags &= ~CRMOD;
	
	sgp->sg_flags &= ~TBDELAY;
	if (tp->c_oflag & XTAB)
		sgp->sg_flags |= XTABS;
	
	sgp->sg_flags &= ~(RAW | CBREAK);
	if (!(tp->c_lflag & ICANON)) {
		if (tp->c_lflag & ISIG)
			sgp->sg_flags |= CBREAK;
		else
			sgp->sg_flags |= RAW;
	}
	
	/* To special chars */
	
	tcp->t_intrc = tp->c_cc[VINTR];
	tcp->t_quitc = tp->c_cc[VQUIT];
	tcp->t_startc = tp->c_cc[VSTART];
	tcp->t_stopc = tp->c_cc[VSTOP];
	tcp->t_eofc = tp->c_cc[VEOF];
	tcp->t_brkc = tp->c_cc[VEOL];
	
	/* To local special chars */
	
	ltcp->t_suspc = tp->c_cc[VSUSP];
	
	/* Turn off extensions... */
	
	if (!(tp->c_lflag & IEXTEN)) {
		ltcp->t_dsuspc = 0xff;
		ltcp->t_rprntc = 0xff;
		ltcp->t_flushc = 0xff;
		ltcp->t_werasc = 0xff;
		ltcp->t_lnextc = 0xff;
	}
	
	/* To local mode word */
	
	if (tp->c_lflag & ECHOE)
		*lmp |= LCRTERA;
	if (tp->c_lflag & ECHOK)
		*lmp |= LCRTKIL;
	if (tp->c_lflag & NOFLSH)
		*lmp |= LNOFLSH;
	if (!(tp->c_oflag & OPOST) && (tp->c_iflag & ICRNL))
		*lmp |= LLITOUT;
	if (!(tp->c_lflag & TOSTOP))
		*lmp |= LTOSTOP;
}
#endif /* not SYSV */
