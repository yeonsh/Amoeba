/*	@(#)tc_tosysv.c	1.2	94/04/07 11:14:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Convert a termios structure to a SYSV termio structure */

/* XXX This is a hack.  See tc_fromsysv.c */

#ifdef SYSV /* This function should only be used under SYSV */

#include "posix/termios.h"

void
tc_tosysv(tp, sysv_tp)
	struct termios *tp;
	char *sysv_tp;
{
	* (struct termios *) sysv_tp = *tp;
#ifdef sgi
	/* SGI-specific extra hacks */
	sysv_tp[24+5] = tp->c_cc[VSTART];
	sysv_tp[24+4] = tp->c_cc[VSTOP];
#endif /* sgi */
}

#else /* SYSV */

#ifndef lint

static int __NOT_USED;

#endif /* lint */

#endif /* SYSV */
