/*	@(#)tty.c	1.2	96/02/27 12:44:49 */
/*
 * tty.c - Return tty name - under Amoeba this is a magic name /dev/tty
 *         in most cases.
 *
 *	Author: Gregory J. Sharp, Nov 1995
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


main(argc, argv)
int	argc;
char *	argv[];
{
    char *	tty_name;

    tty_name = ttyname(0);
    if (argc == 2)
    {
	if (strcmp(argv[1], "-s") != 0)
	{
	    fprintf(stderr, "Usage: %s [-s]\n", argv[0]);
	}
	/* Else silent mode */
    }
    else
    {
	printf("%s\n", ((tty_name != NULL) ? tty_name : "not a tty"));
    }
    exit((tty_name != NULL) ? 0 : 1);
}
