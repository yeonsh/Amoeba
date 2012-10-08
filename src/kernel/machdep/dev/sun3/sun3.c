/*	@(#)sun3.c	1.4	94/04/06 09:27:24 */
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

#ifndef NOTTY

#define NTTY            3                       /* number of terminals */
#define NTTYTASK        (2*NTTY)

uint16 ntty      = NTTY;
uint16 nttythread = NTTYTASK;

#include "tty/tty.h"

/* Initial speeds of the ttys */
short tty_speeds[] = {
        B_9600,
        B_9600,
        B_9600,
};

#endif /* NOTTY */
