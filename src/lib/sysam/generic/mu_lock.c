/*	@(#)mu_lock.c	1.2	94/04/07 11:17:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/mutex.h"

void
mu_lock(s)
mutex *	s;
{
    while (mu_trylock(s, -1L) < 0)
	;
}
