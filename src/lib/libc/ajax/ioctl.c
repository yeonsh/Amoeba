/*	@(#)ioctl.c	1.3	94/04/07 10:28:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* BSD ioctl() replacement on top of Posix termios */

#include <sgtty.h> /* From BSD */
#include <termios.h> /* From Posix */

#include <errno.h>
#include <string.h>

void tc_tobsd();
void tc_frombsd();

int
ioctl(i, cmd, arg)
	int i;
	long cmd; /* Long in 4.3BSD, int in Ultrix2.2/3.0; what to do? */
	char *arg;
{
	struct termios tio;
	struct {
		struct sgttyb sg;
		struct tchars tc;
		struct ltchars ltc;
		int localmode;
	} s;
	int optaction;
	
	/* If we fall through the switch, we need to get the attributes */
	switch (cmd) {
	case TIOCGWINSZ:
		return (tcgetwsize(i, (struct winsize *) arg) < 0) ? -1 : 0;
	}
	
	if (tcgetattr(i, &tio) < 0)
		return -1;
	(void) memset((_VOIDSTAR) &s, 0, sizeof s);
	tc_tobsd(&tio, &s.sg, &s.tc, &s.ltc, &s.localmode);
	
	/* If we fall through the switch, we must set the new attributes */
	
	optaction = TCSANOW;
	switch (cmd) {
	
	/* Basic sgttyb */
	
	case TIOCGETP:
		*(struct sgttyb *)arg = s.sg;
		return 0;
	case TIOCSETP:
		s.sg = *(struct sgttyb *)arg;
		optaction = TCSAFLUSH;
		break;
	case TIOCSETN:
		s.sg = *(struct sgttyb *)arg;
		break;
	
	/* Special chars */
	
	case TIOCGETC:
		*(struct tchars *)arg = s.tc;
		return 0;
	case TIOCSETC:
		s.tc = *(struct tchars *)arg;
		break;
	
	/* Local special chars */
	
	case TIOCGLTC:
		*(struct ltchars *)arg = s.ltc;
		return 0;
	case TIOCSLTC:
		s.ltc = *(struct ltchars *)arg;
		break;
	
	/* Local mode */
	
	case TIOCLGET:
		*(int *)arg = s.localmode;
		return 0;
	case TIOCLSET:
		s.localmode = *(int *)arg;
		break;
	case TIOCLBIS:
		s.localmode |= *(int *)arg;
		break;
	case TIOCLBIC:
		s.localmode &= ~ *(int *)arg;
		break;
	
	/* Not recognized */
	
	default:
		errno = ENOTTY;
		return -1;
	
	}
	
	/* We fell through the switch, set the new attributes */
	
	tc_frombsd(&tio, &s.sg, &s.tc, &s.ltc, &s.localmode);
	return tcsetattr(i, optaction, &tio);
}
