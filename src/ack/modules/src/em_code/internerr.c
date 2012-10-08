/*	@(#)internerr.c	1.1	91/04/11 11:58:54 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include <system.h>

C_internal_error()
{
	sys_write(STDERR,"internal error\n",15);
	sys_stop(S_EXIT);
}
