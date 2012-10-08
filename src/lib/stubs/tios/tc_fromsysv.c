/*	@(#)tc_fromsysv.c	1.2	94/04/07 11:13:54 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Convert a SYSV termio structure to a termios structure */

/* XXX This is a hack that only works because the bit assignments
   in Amoeba's <termios.h> "happen" to have the same values as those
   in SYSV's <termio.h>.
   We really need to convert one structure into another without making
   such an assumption,  but the files that define the respective
   structures define mostly the same symbols.  The cool way to do this
   is to hack Amoeba's termios.h to define values of the symbols with a
   prefix _AMOEBA_ and to use those symbols instead.  Later.  --Guido */

#ifdef SYSV /* This function should only be used under SYSV */

#include "posix/termios.h"

void
tc_fromsysv(tp, sysv_tp)
	struct termios *tp;
	char *sysv_tp;
{
	*tp = * (struct termios *) sysv_tp;
#ifdef sgi
	/* SGI-specific extra hacks */
	tp->c_cc[VSTART] = sysv_tp[24+5];
	tp->c_cc[VSTOP] = sysv_tp[24+4];
#endif /* sgi */
}

#else

#ifndef lint

static int __NOT_USED;

#endif /* lint */

#endif /* SYSV */
