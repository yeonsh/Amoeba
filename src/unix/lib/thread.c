/*	@(#)thread.c	1.3	96/02/27 11:56:09 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "thread.h"

thread_newthread(func, stsize, param, psize)
void	(*func)();
char *	param;
{
    return 0;
}

thread_exit()
{
    exit(0);
    /*NOTREACHED*/
}
