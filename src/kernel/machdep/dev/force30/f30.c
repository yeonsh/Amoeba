/*	@(#)f30.c	1.3	94/04/06 09:03:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "map.h"

/************************************************************************/
/*      TTY                                                             */
/************************************************************************/

#include "ua68562info.h"
struct uart68562info uart68562info = {
        DVADR_UA68562,          /* device adress in mapped space */
        244                      /* vector (MUST be 244 for this board) */
};

#ifndef NOTTY

#define NTTY            1                       /* number of terminals */
#define NTTYTASK        (2*NTTY)

uint16 ntty      = NTTY;
uint16 nttythread = NTTYTASK;

#include "tty/tty.h"

/* Initial speeds of the ttys */
short tty_speeds[] = {
        B_9600,
};

#endif /* NOTTY */

/************************************************************************/
/*      TIMER                                                           */
/************************************************************************/

#include "pit68230info.h"

struct pit68230info pit68230info = {
        DVADR_PT68230,
        242
};

#ifdef PROFILE

struct pit68230info pit2_68230info = {
	DVADR_PT2_68230,
	243,
};

#endif /* PROFILE */
