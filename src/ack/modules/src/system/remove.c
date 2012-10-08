/*	@(#)remove.c	1.1	91/04/11 13:49:51 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

int
sys_remove(path)
	char *path;
{
	return unlink(path) == 0;
}
