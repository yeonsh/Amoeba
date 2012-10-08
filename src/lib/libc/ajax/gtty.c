/*	@(#)gtty.c	1.2	94/04/07 10:28:24 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <sgtty.h>

int
gtty(fd, pbuf)
	int fd;
	struct sgttyb *pbuf;
{
	return ioctl(fd, TIOCGETP, pbuf);
}
