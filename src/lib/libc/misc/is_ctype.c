/*	@(#)is_ctype.c	1.2	94/04/07 10:48:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef __STDC__
/* ANSI requires isalpha etc., also to be available as function */

#include	<ctype.h>

#define FUNCDEF(isxx)	int (isxx)(int c) { return isxx(c); }

FUNCDEF(isalpha)
FUNCDEF(isspace)
FUNCDEF(iscntrl)
FUNCDEF(isxdigit)
FUNCDEF(isalnum)
FUNCDEF(isgraph)
FUNCDEF(ispunct)
FUNCDEF(isdigit)
FUNCDEF(islower)
FUNCDEF(isupper)
FUNCDEF(isprint)
FUNCDEF(isascii)

#else

#ifndef lint
static int file_may_not_be_empty;
#endif

#endif /* __STDC__ */
