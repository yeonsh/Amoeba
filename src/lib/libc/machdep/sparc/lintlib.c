/*	@(#)lintlib.c	1.2	94/04/07 10:42:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "setjmp.h"

/*ARGSUSED*/
int
setjmp(env)
jmp_buf env;
{
    return 0;
}

/*ARGSUSED*/
void
longjmp(env, val)
jmp_buf env;
int     val;
{
}
