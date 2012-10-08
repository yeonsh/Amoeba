/*	@(#)chmode.c	1.1	91/04/11 13:48:26 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

int
sys_chmode(path, mode)
	char *path;
	int mode;
{
	return chmod(path, mode) == 0;
}
