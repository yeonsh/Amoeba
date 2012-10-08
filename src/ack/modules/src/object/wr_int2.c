/*	@(#)wr_int2.c	1.1	91/04/11 13:04:15 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include "object.h"

wr_int2(fd, i)
{
	char buf[2];

	put2(i, buf);
	wr_bytes(fd, buf, 2L);
}
