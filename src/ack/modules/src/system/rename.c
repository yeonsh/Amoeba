/*	@(#)rename.c	1.1	91/04/11 13:50:01 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

int
sys_rename(path1, path2)
	char *path1, *path2;
{
	unlink(path2);
	return	link(path1, path2) == 0 &&
		unlink(path1) == 0;
}

