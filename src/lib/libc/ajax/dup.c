/*	@(#)dup.c	1.3	94/04/07 10:25:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* dup(2) system call emulation */

#include <unistd.h>
#include <fcntl.h>

int
dup(fd)
int fd;
{
	return fcntl(fd, F_DUPFD, 0);
}
