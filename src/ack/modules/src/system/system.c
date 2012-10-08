/*	@(#)system.c	1.1	91/04/11 13:50:30 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <system.h>

File _sys_ftab[SYS_NOPEN] = {
	{ 0, OP_READ},
	{ 1, OP_APPEND},
	{ 2, OP_APPEND}
};

File *
_get_entry()
{
	register File *fp;

	for (fp = &_sys_ftab[0]; fp < &_sys_ftab[SYS_NOPEN]; fp++)
		if (fp->o_flags == 0)
			return fp;
	return (File *)0;
}
