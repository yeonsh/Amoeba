/*	@(#)chm.c	1.4	94/04/07 16:05:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"

#include "stdlib.h"
#include "stdio.h"


static int
get_mask(mask, nmasks, maskar)
char *	mask;	/* in: the string containing column masks */
int	nmasks;	/* in: initial length of maskar */
long *	maskar;	/* out: the value of the mask in the environment */
{
    register	i;
    char *	p;
    long	maskval;

    if (nmasks > SP_MAXCOLUMNS)
	    nmasks = SP_MAXCOLUMNS;

    for (i = 0;;)
    {
	maskval = strtol(mask, &p, 16);
	if (p == mask)
	    break;
	if (i >= nmasks)
	    break;
	maskar[i++] = maskval;
	if (*p != ':')
	    break;
	mask = p+1;
    }
    return i; /* number of columns filled in */
}


main(argc, argv)
int	argc;
char *	argv[];
{
    int		n;	/* number of columns in mask specified on cmd-line */
    int		i;
    long	cols[SP_MAXCOLUMNS];
    errstat	err;

    if (argc < 3)
    {
	fprintf(stderr, "Usage: %s mask name ...\n", argv[0]);
	exit(1);
    }
    n = get_mask(argv[1], SP_MAXCOLUMNS, cols);
    if (n <= 0 || n > SP_MAXCOLUMNS)
    {
	fprintf(stderr, "mask is of the form nn[:nn]*  eg. ff:2:4\n");
	exit(1);
    }
    for (i = 2; i < argc; i++)
    {
	if ((err = sp_chmod(SP_DEFAULT, argv[i], n, cols)) != STD_OK)
	{
	    fprintf(stderr, "%s: can't chmod %s (%s)\n",
					    argv[0], argv[i], err_why(err));
	    exit(1);
	}
    }
    exit(0);
    /*NOTREACHED*/
}
