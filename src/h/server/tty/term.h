/*	@(#)term.h	1.3	94/04/06 17:16:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TERM_H__
#define __TERM_H__

#include "tty/tty.h"

#define QUEUESIZE		1024	/* size of internal queues */
#define NINQUEUE		 256	/* input queue size */
#define BMSIZE		     (256/8)	/* size of bitmap */

#define CNTRL(c)	(c & 037)

/* The following two were defined as something illegal in type.h. */
#undef unsigned
#undef int

#define isprint(c)	(((unsigned)(((c)&0xFF)-' '))<='~'-' ')
#define isupper(c)	(((unsigned)(((c)&0xFF)-'A'))<='Z'-'A')
#define islower(c)	(((unsigned)(((c)&0xFF)-'a'))<='z'-'a')
#if MAPCASES
#define tolower(c)	((c)-'A'+'a')
#define toupper(c)	((c)-'a'+'A')
#endif /* MAPCASES */

extern uint16 intsleeping;

extern struct connect {
	uint16 con_ttynr;
	int (*con_func)();
} *connect;

/* Use int here since many compilers do not know about void. */
struct cdevsw {
	int (*cd_write)();		/* write a character */
#if !VERY_SIMPLE || RTSCTS
	int (*cd_isusp)();		/* try to suspend input */
	int (*cd_iresume)();		/* input can come now */
#endif /* !VERY_SIMPLE || RTSCTS */
#if !VERY_SIMPLE && !NO_IOCTL
	int (*cd_baud)();		/* set baud rate */
	int (*cd_setDTR)();		/* set or clear DTR bit in hardware */
	int (*cd_setBRK)();		/* set or clear break bit in hardware */
#endif /* !VERY_SIMPLE && !NO_IOCTL */
	int (*cd_status)();		/* status info about line */
};

struct queue {
	char q_data[QUEUESIZE];
#if !VERY_SIMPLE
	char *q_extra;
	uint16 q_litt[QUEUESIZE / (sizeof (uint16) * 8)];
#endif /* !VERY_SIMPLE */
	uint16 q_nchar;
	char *q_head;
	char *q_tail;
};

struct io {
	struct queue io_oqueue;		/* output queue */
	struct queue io_iqueue;		/* input queue */
	struct cdevsw *io_funcs;	/* driver dependent functions */
	port io_random;			/* random part of capability */
	port io_newrand;		/* new random part of capability */
#if !VERY_SIMPLE && !NO_IOCTL
	char io_flags[NFLAGS];		/* I/O flags */
#endif /* !VERY_SIMPLE && !NO_IOCTL */
	long io_state;			/* internal status */
#if !VERY_SIMPLE
#if !NO_IOCTL
	short io_speed;			/* baud rate of terminal */
#endif /* !NO_IOCTL */
	short io_nbrk;	      		/* # of break chars in input queue */
	short io_icol;			/* column of beg. of pending input */
	short io_ocol;			/* column of cursor on output */
	capability io_intcap;		/* where to send interrupts */
	char io_intr_char;		/* interrupt character (SIGINTR) */
	char io_quit_char;		/* quit character (SIGQUIT) */
	char io_susp_char;		/* suspend character (SIGSUSP) */
	char io_intr[BMSIZE];		/* bitmap of interrupt characters */
	char io_ints[BMSIZE];		/* bitmap of typed intr chars */
	char io_ign[BMSIZE];		/* bitmap of chars to be ignored */
	char io_nprint[BMSIZE];		/* bitmap of chars not to be printed */
	char io_eof[BMSIZE];		/* end-of-file characters */
	char io_brk[BMSIZE];		/* input delimiters (incl. NL) */
	uint16 io_tabw;			/* width of a tab */
	char io_erase;			/* erase character */
	char io_werase;			/* word erase */
	char io_kill;			/* kill line */
	char io_rprnt;			/* reprint line */
	char io_flush;			/* flush output (toggles) */
	char io_lnext;			/* literal next character */
	char io_istop;			/* stop input */
	char io_istart;			/* start input */
	char io_ostop;			/* stop output */
	char io_ostart;			/* start output */
#endif /* !VERY_SIMPLE */
};

extern struct io *tty;
extern char *ttythread;
extern uint16 ntty, nttythread;

#define setbit(ptr, bitnr)	((ptr)[(bitnr) >> 3] |= (1 << ((bitnr) & 07)))
#define clrbit(ptr, bitnr)	((ptr)[(bitnr) >> 3] &= ~(1 << ((bitnr) & 07)))
#define tstbit(ptr, bitnr)	((ptr)[(bitnr) >> 3] & (1 << ((bitnr) & 07)))

#if !VERY_SIMPLE
#if NO_IOCTL
#define tstflag(iop, b)		((b)==_ISTRIP  ||(b)==_ICRNL  ||(b)==_IXON   ||\
				 (b)==_IXANY   ||(b)==_ISIG   ||(b)==_ICANON ||\
				 (b)==_WERASECH||(b)==_ECHO   ||(b)==_RPRNTCH||\
				 (b)==_LNEXTCH ||(b)==_FLUSHCH||(b)==_CRTBS  ||\
				 (b)==_CRTERA  ||(b)==_CRTKIL ||(b)==_CTLECHO||\
				 (b)==_OPOST   ||(b)==_ONLCR  ||(b)==_XTAB)
#else
#define setflag(iop, bitnr)	setbit((iop)->io_flags, (bitnr))
#define clrflag(iop, bitnr)	clrbit((iop)->io_flags, (bitnr))
#define tstflag(iop, bitnr)	tstbit((iop)->io_flags, (bitnr))
#endif /* NO_IOCTL */
#endif /* !VERY_SIMPLE */

#define Lbit(b)			(1L << (b))

/* Bits describing the internal state */

#define SQUOT		Lbit( 0)	/* last character was \ */
#define SERASE		Lbit( 1)	/* within \.../ for PRTERA */
#define SLNCH		Lbit( 2)	/* next character is literal */
#define STYPEN		Lbit( 3)	/* retyping suspended input (PENDIN) */
#define SXTAB		Lbit( 4)	/* expanding a tab */
#define SISUSP		Lbit( 5)	/* input is suspended */
#define SOSUSP		Lbit( 6)	/* output is suspended */
#define SOWRITE		Lbit( 7)	/* writing a character */
#define SOLF		Lbit( 8)	/* writing the CR part of CR-LF */
#define SDIRTY		Lbit( 9)	/* output when there's pending input */
#define SCNTTB		Lbit(10)	/* counting the width of a tab */
#define SFLUSH		Lbit(11)	/* flushing output */
#define SINTED		Lbit(12)	/* interrupt char typed */
#if MAPCASES
#define SBKSL		Lbit(13)	/* state bit for lowercase backslash work */
#endif /* MAPCASES */
#define SISIGNAL	Lbit(14)	/* input thread got signal */
#define SOSIGNAL	Lbit(15)	/* output thread got signal */
#define SSTOPINPUT	Lbit(16)	/* suspend input */
#define SSTARTINPUT	Lbit(17)	/* resume input */
#if NO_IOCTL
#define SFLUSHO		Lbit(18)	/* output being flushed (see _FLUSHO) */
#endif /* NO_IOCTL */
#define SOSLEEP		Lbit(19)	/* sleeping for output to drain */
#define SHANGUP		Lbit(20)	/* terminal hung up */
#define SINTRUP		Lbit(21)	/* send TTI_INTRUP interrupt */
#define SNEWRAND	Lbit(22)	/* new random number in io_newrand */

#if VERY_SIMPLE || NO_IOCTL

#define LOWATER(iop)		50
#define HIWATER(iop)		200

#else

extern uint16 hiwater[];
extern uint16 lowater[];

#define LOWATER(iop)		lowater[(iop)->io_speed]
#define HIWATER(iop)		hiwater[(iop)->io_speed]

#endif /* VERY_SIMPLE || NO_IOCTL */

#ifdef USART
#define FIRST_iSBC	1
#endif /* USART */

#ifdef CDC
#define FIRST_CDC	5
#endif /* CDC */

#endif /* __TERM_H__ */
