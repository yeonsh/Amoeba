/*	@(#)termios.h	1.3	94/04/06 16:54:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix 1003.1 compliant terminal interface */

#ifndef __TERMIOS_H__
#define __TERMIOS_H__

#ifdef ail
#define NOPROTO
#endif

#ifndef NOPROTO
#include "_ARGS.h"
#endif

/* Avoid conflicts with symbols from Unix termio.h in some applications */

#undef ECHO
#undef TOSTOP
#undef NOFLSH
#undef TCSANOW
#undef TCSADRAIN


/* 7.1 General Terminal Interface */

/* The values here are an attempt to be binary compatible with SysV */

/* 7.1.2 Settable Parameters */

/* 7.1.2.1 termios structure */

typedef unsigned short	tcflag_t;
typedef unsigned char	cc_t;

#define NCCS 12

struct termios {
	tcflag_t	c_iflag;	/* Input modes */
	tcflag_t	c_oflag;	/* Output modes */
	tcflag_t	c_cflag;	/* Control modes */
	tcflag_t	c_lflag;	/* Local modes */
	cc_t		c_cc[NCCS];	/* Control characters */
};

/* 7.1.2.2 Input Modes */

#define BRKINT	0x0002	/* Signal interrupt on break */
#define ICRNL	0x0100	/* Map CR to NL on input */
#define IGNBRK	0x0001	/* Ignore break condition */
#define IGNCR	0x0080	/* Ignore CR */
#define IGNPAR	0x0004	/* Ignore characters with parity errors */
#define INLCR	0x0040	/* Map NL to CR on input */
#define INPCK	0x0010	/* Enable input parity check */
#define ISTRIP	0x0020	/* Strip character */
#define IXOFF	0x1000	/* Enable start/stop input protocol */
#define IXON	0x0400	/* Enable start/stop output protocol */
#define PARMRK	0x0008	/* Mark parity errors */

/* 7.1.2.3 Output Modes */

#define OPOST	0x0001	/* Perform output processing */

/* Extended Output Modes, selected by IEXTEN below */

#define XTAB	0x0002	/* Expand tabs on output */

/* 7.1.2.4 Control Modes */

#define CLOCAL	0x0800	/* Ignore modem status lines */
#define CREAD	0x0080	/* Enable receiver */
#define CSIZE	0x0030	/* Number of bits per byte */
#define  CS5	0x0000	/* 5 bits */
#define  CS6	0x0010	/* 6 bits */
#define  CS7	0x0020	/* 7 bits */
#define  CS8	0x0030	/* 8 bits */
#define CSTOPB	0x0040	/* Send two stop bits, else one */
#define HUPCL	0x0400	/* Hang up on last close */
#define PARENB	0x0100	/* Parity enable */
#define PARODD	0x0200	/* Odd parity, else even */

/* 7.1.2.5 Local Modes */

#define ECHO	0x0008	/* Enable echo */
#define ECHOE	0x0010	/* Echo ERASE as an error-correcting backspace */
#define ECHOK	0x0020	/* Echo KILL */
#define ECHONL	0x0040	/* Echo '\n' */
#define ICANON	0x0002	/* Canonical input (erase and kill processing) */
#define IEXTEN	0x0100	/* Enable extended (impl.-defined) functions */
#define ISIG	0x0001	/* Enable signals */
#define NOFLSH	0x0080	/* Disable flush after interrupt, quit or suspend */
#if 1
#define TOSTOP	(int)0x400000	/* SIGSTOP on background output */
#else
#define TOSTOP	0x400000 /* Send SIGTTOU for background output */
#endif

/* 7.1.2.6 Special Control Characters */

/* The values are one higher than for SysV, but since the offset of c_cc
   in the array is one less, things end up at the same offset... */

#define VEOF    5
#define VEOL    6
#define VERASE  3
#define VINTR   1
#define VKILL   4
#define VMIN    9		/* Non-Canonical */
#define VQUIT   2
#define VSUSP	11
#define VTIME   10		/* Non-Canonical */
#define VSTART	7
#define VSTOP	8

#ifndef _POSIX_VDISABLE		/* May also be defined in <unistd.h> */
/* This doesn't belong here but is used by the kernel tty driver. */
#define _POSIX_VDISABLE		((cc_t)0xff)
#endif

/* 7.1.2.7 Baud Rate Functions */

typedef unsigned short speed_t;

#define cfgetospeed(tp)		((tp)->c_cflag & 0xf)
#define cfsetospeed(tp, speed)	\
	((tp)->c_cflag &= ~0xf, (tp)->c_cflag |= (speed) & 0xf, 0)

#define cfgetispeed(tp)		cfgetospeed(tp)
#define cfsetispeed(tp, speed)	((speed) == 0 ? 0 : cfsetospeed(tp, speed))

#define B0	0
#define B50	1
#define B75	2
#define B110	3
#define B134	4
#define B150	5
#define B200	6
#define B300	7
#define B600	8
#define B1200	9
#define B1800	10
#define B2400	11
#define B4800	12
#define B9600	13
#define B19200	14
#define B38400	15


/* 7.2 General Terminal Interface Control Functions */

/* 7.2.1 Get and Set State */

#ifndef NOPROTO
int tcgetattr	_ARGS((int _fildes, struct termios *_tp));
int tcsetattr	_ARGS((int _fildes, int _optional_actions,
		       struct termios *_tp));
#endif

/* Optional actions for tcsetattr(): */
#define TCSANOW		1
#define TCSADRAIN	2
#define TCSAFLUSH	3

/* 7.2.2 Line Control Functions */

#ifndef NOPROTO
int tcsendbreak	_ARGS((int _fildes, int _duration));
int tcdrain	_ARGS((int _fildes));
int tcflush	_ARGS((int _fildes, int _queue_selector));
int tcflow	_ARGS((int _fildes, int _action));
#endif

/* Queue selector for tcflush(): */
#define TCIFLUSH	1
#define TCOFLUSH	2
#define TCIOFLUSH	(TCIFLUSH | TCOFLUSH)

/* Action for tcflow(): */
#define TCOOFF		1
#define TCOON		2
#define TCIOFF		3
#define TCION		4

#endif /* __TERMIOS_H__ */
