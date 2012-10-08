/*	@(#)setjmp.c	1.2	94/04/07 10:36:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <setjmp.h>

setjmp(env)
jmp_buf env;
{
	/* probably in assembler */
}

longjmp(env, val)
jmp_buf env;
{
	/* probably in assembler */
}
