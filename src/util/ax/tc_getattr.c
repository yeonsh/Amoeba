/*	@(#)tc_getattr.c	1.3	96/02/27 13:07:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* An implementation of Posix tcgetattr() on top of BSD ioctls.
   It is called tc_getattr() to avoid possibly name conflicts in the library.
   It includes "posix/termios.h" from the amoeba include tree because it
   requires compatibility with Amoeba's termios struct.
*/

#ifndef SYSV

#include <sgtty.h>
#include "posix/termios.h"

#ifdef __STDC__
#include <stdlib.h>
#else
#define	memset(a, b, c)		bzero(a, c)
#endif

int
tc_getattr(fd, tp)
	int fd;
	struct termios *tp;
{
	void tc_frombsd();

	struct sgttyb sg;
	struct tchars tc;
	struct ltchars ltc;
	int localmode;
	
	if (ioctl(fd, TIOCGETP, &sg) < 0 || ioctl(fd, TIOCGETC, &tc) < 0)
		return -1;
	if (ioctl(fd, TIOCGLTC, &ltc) < 0) {
		(void) memset(&ltc, '\0', sizeof ltc);
	}
	if (ioctl(fd, TIOCLGET, &localmode) < 0)
		localmode = 0;
	tc_frombsd(tp, &sg, &tc, &ltc, &localmode);
	return 0;
}

#endif /* SYSV */
