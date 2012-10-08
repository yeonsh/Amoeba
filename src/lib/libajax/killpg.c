/*	@(#)killpg.c	1.2	94/04/07 09:48:24 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* killpg(2) system call emulation */

#include "ajax.h"
#include "sesstuff.h"

int
killpg(pgrp, sig)
	int pgrp, sig;
{
	return kill(-pgrp, sig);
}
