/*	@(#)tios.h	1.3	96/02/27 10:29:32 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __TIOS_H__
#define __TIOS_H__

#if defined(AMOEBA) || !defined(SYSV)
#include "posix/termios.h"
#else
#include <termios.h>
#endif

/* Constants: */
#define TIOS_GETATTR 3200
#define TIOS_SETATTR 3201
#define TIOS_SENDBREAK 3202
#define TIOS_DRAIN 3203
#define TIOS_FLUSH 3204
#define TIOS_FLOW 3205
#define TIOS_GETWSIZE 3206
#define TIOS_SETWSIZE 3207

/* Operators: */
extern errstat tios_getattr();
extern errstat tios_setattr();
extern errstat tios_sendbrk();
extern errstat tios_drain();
extern errstat tios_flush();
extern errstat tios_flow();
extern errstat tios_getwsize();
extern errstat tios_setwsize();

#endif /* __TIOS_H__ */
