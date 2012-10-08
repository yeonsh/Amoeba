/*	@(#)sigpause.c	1.2	94/04/07 09:52:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sigpause(2) system call emulation */
/* TO DO: use the mask */

#include "ajax.h"

int
sigpause(mask)
	long mask;
{
	mutex mu;
	
	mu_init(&mu);
	mu_lock(&mu);
	mu_trylock(&mu, -1);
	mu_unlock(&mu);
	ERR(EINTR, "sigpause: interrupted");
}
