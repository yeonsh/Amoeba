/*	@(#)mkdir.c	1.1	91/04/24 17:36:04 */
#include <stdio.h>
#include <stdlib.h>

int     error = 0;

main (argc, argv)
register int argc;
register char  **argv;
{
    if (argc < 2) {
	printf ("Usage: mkdir dir ...\n");
	exit (1);
    }
    while (--argc)
	makedir (*++argv);
    exit (error);
}

makedir (dirname)
char   *dirname;
{
    if (mkdir (dirname, 0777) < 0) {
	perror (dirname);
	error++;
    }
}
