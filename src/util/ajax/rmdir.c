/*	@(#)rmdir.c	1.1	91/04/24 17:37:18 */
/*
**	rmdir - remove a directory
**
** Author: Gregory J. Sharp, 12/04/91
*/

#include <stdio.h>
#include <stdlib.h>

main(argc, argv)
int argc;
char **argv;
{
    int	error;
    char *dirname;

    if (argc < 2)
    {
	fprintf(stderr, "Usage: %s dir ...\n", argv[0]);
	exit (1);
    }
    error = 0;
    while (--argc)
    {
	dirname = *++argv;
	if (rmdir(dirname) < 0)
	{
	    perror(dirname);
	    error++;
	}
    }
    exit(error);
}
