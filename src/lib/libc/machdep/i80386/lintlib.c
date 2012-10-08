/*	@(#)lintlib.c	1.3	94/04/07 10:38:41 */
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

/*ARGSUSED*/
char *
alloca(size)
int size;
{
    return 0;
}

/*ARGSUSED*/
bcopy(s, d, c)
char *	s;
char *	d;
int	c;
{
}

/*ARGSUSED*/
bzero(s, c)
char *	s;
int	c;
{
}

/*ARGSUSED*/
bcmp(s1, s2, n)
char *	s1;
char *	s2;
int	n;
{
}
