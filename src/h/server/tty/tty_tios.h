/*	@(#)tty_tios.h	1.2	94/04/06 17:16:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TTY_TIOS_H__
#define __TTY_TIOS_H__

/*
** tty_tios.h, defintions used for the tios implementation of the tty-server.
** 
** Peter Bosch, 260190, peterb@cwi.nl;
*/

/* 
** commands from the interrupt thread 
**
*/
#define TIOS_INTERRUPT	(TIOS_FIRST_COM + 1)	/* interrupt character typed */

#endif /* __TTY_TIOS_H__ */
