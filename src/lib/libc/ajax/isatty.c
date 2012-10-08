/*	@(#)isatty.c	1.2	94/04/07 10:28:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix isatty() function */

#include <termios.h>

int
isatty(fd)
	int fd;
{
	struct termios t;
	
	return tcgetattr(fd, &t) == 0;
}
