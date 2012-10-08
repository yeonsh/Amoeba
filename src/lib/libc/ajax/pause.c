/*	@(#)pause.c	1.2	94/04/07 10:30:45 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* pause(2) system call emulation */

#include "ajax.h"

int
pause()
{
	mutex mu;
	
	mu_init(&mu);
	mu_lock(&mu);
	mu_trylock(&mu, -1);
	mu_unlock(&mu);
	ERR(EINTR, "pause: interrupted");
}
