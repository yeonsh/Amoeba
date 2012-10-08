/*	@(#)dirname.c	1.3	94/04/07 14:40:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Dirname - Print the path component of the filename specified
 *
 * Author: Gregory J. Sharp, Jan 1994
 */
#include <stdio.h>
#include <string.h>


static void
usage(progname)
char * progname;
{
    fprintf(stderr, "Usage: %s pathname\n", progname);
    exit(-1);
}


main(argc, argv)
int argc;
char * argv[];
{
    char * p;

    if (argc != 2)
    {
	usage(argv[0]);
	/*NOTREACHED*/
    }
    /* Delete trailing / characters */
    p = argv[1] + strlen(argv[1]) - 1;
    while (p > argv[1] && *p == '/')
    {
	*p = '\0';
	p--;
    }

    /* Now work out what the directory name is */
    p = strrchr(argv[1], '/');
    if (p == NULL)
	printf(".\n");
    else
    {
	*p = '\0';
	/* Just in case the dirname is / */
	if (argv[1][0] == '\0')
	    argv[1] = "/";
	printf("%s\n", argv[1]);
    }
    exit(0);
}
