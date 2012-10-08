/*	@(#)tc_setattr.c	1.3	96/02/27 13:07:54 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* An implementation of Posix tcsetattr() on top of BSD ioctls.
   It is called tc_setattr() to avoid possibly name conflicts in the library.
   It includes "posix/termios.h" from the amoeba include tree because it
   requires compatibility with Amoeba's termios struct. */

#ifdef SYSV

#include <termios.h>

#else /* ! SYSV */
#include <sgtty.h>
#include "posix/termios.h"

#ifdef __STDC__
#include <stdlib.h>
#else
#define	memset(a, b, c)		bzero(a, c)
#endif

#endif /* SYSV */

tc_setattr(fd, optact, tp)
	int fd;
	int optact;
	struct termios *tp;
{
#ifdef SYSV
	switch (optact)
	{
	case 1:
		optact = TCSANOW;
		break;
	case 2:
		optact = TCSADRAIN;
		break;
	default:
		optact = TCSAFLUSH;
		break;
	}
	return tcsetattr(fd, optact, tp);
#else
	void tc_tobsd();

	struct sgttyb sg;
	struct tchars tc;
	struct ltchars ltc;
	int localmode;
	int err;
	
	if (ioctl(fd, TIOCGETP, &sg) < 0 || ioctl(fd, TIOCGETC, &tc) < 0)
		return -1;
	if (ioctl(fd, TIOCGLTC, &ltc) < 0) {
		(void) memset(&ltc, '\0', sizeof ltc);
        }
	if (ioctl(fd, TIOCLGET, &localmode) < 0)
		localmode = 0;
	tc_tobsd(tp, &sg, &tc, &ltc, &localmode);
	if (optact == TCSANOW)
		err = ioctl(fd, TIOCSETN, &sg);
	else
		err = ioctl(fd, TIOCSETP, &sg);
	err += ioctl(fd, TIOCSETC, &tc);
	err += ioctl(fd, TIOCSLTC, &ltc);
	err += ioctl(fd, TIOCLSET, &localmode);
	return err;
#endif /* SYSV */
}
