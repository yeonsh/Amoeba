/*	@(#)isa.c	1.3	94/04/06 09:20:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * isa.c
 */
#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <map.h>

/*
 * Machine dependent tty server definitions
 */
#ifndef NOTTY
/*
 * When the asynchroneous driver is compiled into the kernel we can have
 * up to five terminals (4 asynchroneous interfaces and one console).
 * Otherwise there is only one terminal (the console).
 */
#ifndef ASY
#define NTTY		1
#else
#define NTTY		2
#endif

#define NTTYTASK	(2*NTTY)

uint16 ntty	 = NTTY;
uint16 nttythread = NTTYTASK;

#include "tty/tty.h"

/* Initial speeds of the ttys */
short tty_speeds[] = {
	B_9600,
	B_9600,
};
#endif /* NOTTY */
