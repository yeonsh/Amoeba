/*	@(#)settimeofday.c	1.2	94/04/07 09:51:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* gettimeofday(2) system call emulation (always fails: not su) */

#include "ajax.h"
#include <sys/time.h>

int
settimeofday(tp, tzp)
	struct timeval *tp;
	struct timezone *tzp;
{
	ERR(EPERM, "settimeofday denied");
}
