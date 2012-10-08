/*	@(#)close.c	1.1	91/04/11 13:48:36 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <system.h>

sys_close(fp)
	register File *fp;
{
	if (fp) {
		fp->o_flags = 0;
		close(fp->o_fd);
		fp->o_fd = -1;
	}
}
