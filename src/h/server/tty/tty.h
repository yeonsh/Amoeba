/*	@(#)tty.h	1.3	94/04/06 17:16:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TTY_H__
#define __TTY_H__

/*
 * Due to the most appalling hacks we have to make the tty interface
 * identical to the old file server interface.
 */
#include "file.h"

/*
** prefix of name of ttysver in processor directory
** The suffix :nn is added (n is a digit) for each device
*/
#define	TTY_SVR_NAME	"tty"

/* commands to the terminal server */
#define TTQ_READ	FSQ_READ      /* read request */
#define TTQ_WRITE	FSQ_WRITE     /* write request */
#define TTQ_CLOSE	FSQ_CLOSE     /* close session */
#define TTQ_STATUS	FSQ_STATUS    /* ask status */
#define TTQ_CONTROL	FSQ_CONTROL   /* i/o control */
#define TTQ_INTCAP	FSQ_INTCAP    /* set capability of interrupt handler */
#define TTQ_TIME_READ	FSQ_TIME_READ /* read with timeout */

/* commands from the interrupt thread */
#define TTI_INTERRUPT	(TTY_FIRST_COM + 1) /* interrupt character(s) typed */
#define TTI_HANGUP	(TTY_FIRST_COM + 2) /* terminal hung up */
#define TTI_INTRUP	(TTY_FIRST_COM + 3) /* characters available interrupt */
#define TTI_WINSIZE	(TTY_FIRST_COM + 4) /* window size change */
#define TTI_SIGNAL	(TTY_FIRST_COM + 5) /* Posix signal */

#define BITMASK		0x3F	/* mask to get ioctl command */

#define NFLAGS		6	/* # of bytes needed to store all flags */

/* macros that can be used to set, clear and test the flag values */
#define SetFlag(b, f)		((f)[(b)>>3] |= 1<<((b)&7))
#define ClrFlag(b, f)		((f)[(b)>>3] &= ~(1<<((b)&7)))
#define TstFlag(b, f)		((f)[(b)>>3] & 1<<((b)&7))

/* flag values */
#define _ISTRIP		0x00	/* strip input character to 7 bits */
#define _INLCR		0x01	/* map linefeed to carriage return on input */
#define _ICRNL		0x02	/* map carriage return to linefeed on input */
#define _IXON		0x03	/* enable start/stop output control */
#define _IXANY		0x04	/* any character to restart output */
#define _IXOFF		0x05	/* enable start/stop input control */
#define _ISIG		0x06	/* enable signals */
#define _ICANON		0x07	/* canonical input (line buffering) */
#define _WERASECH	0x08	/* enable word erase */
#define _ECHO		0x09	/* echo input */
#define _RPRNTCH	0x0A	/* enable reprint character */
#define _LNEXTCH	0x0B	/* enable literal next character */
#define _FLUSHCH	0x0C	/* enable flush output character */
#define _CRTBS		0x0D	/* echo erase as backspace */
#define _PRTERA		0x0E	/* printing terminal erase mode */
#define _CRTERA		0x0F	/* erase character echoes as "\b \b" */
#define _CRTKIL		0x10	/* "\b \b" erase on line kill */
#define _CTLECHO	0x11	/* echo control characters as ^X */
#define _PENDIN		0x12	/* retype pending input at next read */
#define _DELAYPROC	0x13	/* delay input processing (not implemented) */
#define _OPOST		0x14	/* postprocess output */
#define _OSTRIP		0x15	/* strip output chars to 7 bits */
#define _ONLCR		0x16	/* map NL to CR-NL on output */
#define _OCRNL		0x17	/* map CR to NL on output */
#define _ONOCR		0x18	/* no CR output at column 0 */
#define _ONLRET		0x19	/* NL performs CR function */
#define _OFILL		0x1A	/* use fill characters for delay (not implemented) */
#define _OFDEL		0x1B	/* fill character is DEL, else NUL (not implemented) */
#define _OFLUSH		0x1C	/* output is being flushed */
#define _XTAB		0x1D	/* expand tab to spaces */
#define _OTILDE		0x1E	/* convert ~ to ` for Hazeltines */
#define _XCASE		0x1F	/* canonical upper/lower presentation (not implemented) */
#define _IUCLC		0x20	/* map upper case to lower case */
#define _OLCUC		0x21	/* map lower case to upper on output */
#define _NOFLUSH	0x22	/* don't flush on interrupt */
#define _IMODEM		0x23	/* RTS/CTS flow control on input */
#define _INTRUP		0x24	/* send interrupt when characters available */

/* Command bits for ioctl calls */
#define _SETFLAG	0x40	/* set a flag bit */
#define _CLRFLAG	0x80	/* clear a flag bit */
#define _SETSPEED	0xC0	/* set baud rate */

#define _SETINTR	0x00	/* interrupt characters follow */
#define _SETIGN		0x01	/* ignore characters follow */
#define _SETNPRNT	0x02	/* characters not to be printed follow */
#define _SETEOF		0x03	/* end of file characters follow */
#define _SETBRK		0x04	/* break characters follow */
#define _SETTAB		0x05	/* set tabwidth */
#define _SETERASE	0x06	/* set erase character */
#define _SETWERASE	0x07	/* set word erase character */
#define _SETKILL	0x08	/* set line kill character */
#define _SETRPRNT	0x09	/* set reprint character */
#define _SETFLUSH	0x0A	/* set flush characters */
#define _SETLNEXT	0x0B	/* set literal next character */
#define _SETISTOP	0x0C	/* set input stop character */
#define _SETISTART	0x0D	/* set input start character */
#define _SETOSTOP	0x0E	/* set output stop character */
#define _SETOSTART	0x0F	/* set output start character */
#define _FLUSH		0x10	/* flush input and output */
#define _SETDTR		0x11	/* data terminal ready is set */
#define _CLRDTR		0x12	/* data terminal ready is cleared */
#define _SETBREAK	0x13	/* the break bit is set in the terminal */
#define _CLRBREAK	0x14	/* the break bit is cleared */
#define _FLAGS		0x15	/* set the ioctl flag values */

/* Indices in baud rate table and possible speeds to be ORed into command */

#define B_50		0x00	/*    50 baud */
#define B_75		0x01	/*    75 baud */
#define B_110		0x02	/*   110 baud */
#define B_134		0x03	/*   134.5 bd */
#define B_150		0x04	/*   150 baud */
#define B_200		0x05	/*   200 baud */
#define B_300		0x06	/*   300 baud */
#define B_600		0x07	/*   600 baud */
#define B_1200		0x08	/*  1200 baud */
#define B_1800		0x09	/*  1800 baud */
#define B_2400		0x0A	/*  2400 baud */
#define B_4800		0x0B	/*  4800 baud */
#define B_9600		0x0C	/*  9600 baud */
#define B_19200		0x0D	/* 19200 baud */

/* Commands for status calls */

#define _NREAD		0x00	/* # of chars in input queue */
#define _NWRITE		0x01	/* # of chars in output queue */
#define _SPEED		0x02	/* line speed */
#define _GETFLAGS	0x03	/* current value of ioctl flags */
#define	_GETWINSZ	0x04	/* current window size */

/* Argument to _FLUSH command (can be ORed together) */

#define _READ		0x01	/* flush input queue */
#define _WRITE		0x02	/* flush output queue */

/* Default values for some compile-time switches */

#ifndef VERY_SIMPLE
#define VERY_SIMPLE	0
#endif
#ifndef NO_IOCTL
#define NO_IOCTL	0
#endif
#ifndef RTSCTS
#define RTSCTS		0
#endif
#ifndef XONXOFF
#define XONXOFF		0
#endif
#ifndef MAPCASES
#define MAPCASES	0
#endif

#endif /* __TTY_H__ */
