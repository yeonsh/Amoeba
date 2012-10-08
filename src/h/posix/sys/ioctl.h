/*	@(#)ioctl.h	1.2	94/04/06 16:56:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __IOCTL_H__
#define __IOCTL_H__

/*
** Some definitions for v7 / BSD ioctl() compatibility.
** Many of the symbols here are not supported, but they are defined
** nevertheless so applications that reference them can be compiled.
** The structure lay-out and flag bit definitions are the same as in
** BSD, but the ioctl codes are not (see if this bites us).
*/

struct sgttyb {
	char	sg_ispeed;
	char	sg_ospeed;
	char	sg_erase;
	char	sg_kill;
	short	sg_flags;
};

#include "termios.h"	/* For B0 .. B38400 */

#if ECHO == 010
#undef ECHO	/* Benign conflict with termios.h */
#endif

#define ALLDELAY 0177400
#define BSDELAY	0100000
#define BS0	0
#define BS1	0100000
#define VTDELAY	0040000
#define FF0	0
#define FF1	0040000
#define CRDELAY	0030000
#define CR0	0
#define CR1	0010000
#define CR2	0020000
#define CR3	0030000
#define TBDELAY	0006000
#define TAB0	0
#define TAB1	0002000
#define TAB2	0004000
#define XTABS	0006000
#define NLDELAY	0001400
#define NL0	0
#define NL1	0000400
#define NL2	0001000
#define NL3	0001400
#define EVENP	0000200
#define ODDP	0000100
#define RAW	0000040
#define CRMOD	0000020
#define ECHO	0000010
#define LCASE	0000004
#define CBREAK	0000002
#define TANDEM	0000001

struct tchars {
	char	t_intrc;
	char	t_quitc;
	char	t_startc;
	char	t_stopc;
	char	t_eofc;
	char	t_brkc;
};

struct ltchars {
	char	t_suspc;
	char	t_dsuspc;
	char	t_rprntc;
	char	t_flushc;
	char	t_werasc;
	char	t_lnextc;
};

struct winsize {
	unsigned short	ws_row;
	unsigned short	ws_col;
	unsigned short	ws_xpixel;
	unsigned short	ws_ypixel;
};

#define LCRTBS		0000001
#define LPRTERA 	0000002
#define LCRTERA 	0000004
#define LTILDE		0000010
#define LMDMBUF 	0000020
#define LLITOUT 	0000040
#define LTOSTOP 	0000100
#define LFLUSHO 	0000200
#define LNOHANG 	0000400
#define LEXTACK 	0001000
#define LCRTKIL 	0002000
#define LPASS8	 	0004000
#define LCTLECH 	0010000
#define LPENDIN 	0020000
#define LDECCTQ 	0040000
#define LNOFLSH 	0100000

#define TIOCGETP	1
#define TIOCSETP	2
#define TIOCSETN	3
#define TIOCGETC	4
#define TIOCSETC	5
#define TIOCGLTC	6
#define TIOCSLTC	7
#define TIOCLGET	8
#define TIOCLSET	9
#define TIOCLBIS	10
#define TIOCLBIC	11

#define TIOCGWINSZ	20
#define TIOCSWINSZ	21

#endif /* __IOCTL_H__ */
