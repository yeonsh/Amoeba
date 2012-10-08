/*	@(#)rd_unsig2.c	1.1	91/04/11 13:03:34 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include "object.h"

unsigned int
rd_unsigned2(fd)
{
	char buf[2];

	rd_bytes(fd, buf, 2L);
	return uget2(buf);
}
