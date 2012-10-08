/*	@(#)failed.c	1.1	91/04/11 11:57:33 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include <system.h>

C_failed()
{
	sys_write(STDERR,"read, write, or open failed\n",28);
	sys_stop(S_EXIT);
}
