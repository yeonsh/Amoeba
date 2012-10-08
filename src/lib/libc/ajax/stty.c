/*	@(#)stty.c	1.2	94/04/07 10:32:12 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <sgtty.h>

int
stty(fd, pbuf)
	int fd;
	struct sgttyb *pbuf;
{
	return ioctl(fd, TIOCSETP, pbuf);
}
