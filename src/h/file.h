/*	@(#)file.h	1.6	96/02/27 10:25:43 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __FILE_H__
#define __FILE_H__

/* generic commands to file-like servers  - this is really the tty interface */
#define FSQ_CREATE	(TTY_FIRST_COM + 51)
#define FSQ_READ	(TTY_FIRST_COM + 52)
#define FSQ_WRITE	(TTY_FIRST_COM + 53)
#define FSQ_LINK	(TTY_FIRST_COM + 54)
#define	FSQ_CLOSE	(TTY_FIRST_COM + 55)	/* tty server only */
#define FSQ_STATUS	(TTY_FIRST_COM + 56)
#define FSQ_CONTROL	(TTY_FIRST_COM + 57)
#define FSQ_COPY	(TTY_FIRST_COM + 58)
#define	FSQ_INTCAP	(TTY_FIRST_COM + 59)	/* tty server only */
#define	FSQ_TIME_READ	(TTY_FIRST_COM + 60)	/* tty server only */

#ifndef ail

/* rights */
#define RGT_CREATE	0x01
#define RGT_READ	0x02
#define RGT_WRITE	0x04
#define RGT_APPEND	0x08
#define RGT_LINK	0x10
#define RGT_UNLINK	0x20
#define RGT_STATUS	0x40
#define RGT_CONTROL	0x80

#endif /* ail */

/* some handy definitions */
#define h_crsize	h_offset	/* initial size in create */
#define h_access	h_extra		/* initial rights in create */

/* h_extra as returned by FSQ_READ */
#define FSE_MOREDATA	0		/* You could get more */
#define FSE_NOMOREDATA	1		/* Hit data boundary */

#define BUFFERSIZE	8192		/* unit buffer size */

#define	fsread	_fsread
#define	fswrite	_fswrite

#ifndef ail

long fsread _ARGS(( capability *cap, long position, char *buf, long size ));
long fswrite _ARGS(( capability *cap, long position, char *buf, long size ));

#endif /* ail */

#endif /* __FILE_H__ */
