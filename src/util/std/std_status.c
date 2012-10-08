/*	@(#)std_status.c	1.4	94/04/07 16:09:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "module/stdcmd.h"

#include "stdio.h"
#include "stdlib.h"

#define	BUFSZ	30000

main(argc, argv)
int	argc;
char *	argv[];
{
    capability	cap;
    errstat	err;
    int		size;
    int		bsize;
    char *	p;

    if (argc != 2)
    {
	fprintf(stderr, "Usage: %s object-pathname\n", argv[0]);
	exit(1);
    }
    if ((err = name_lookup(argv[1], &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
						argv[0], argv[1], err_why(err));
	exit(1);
    }
    p = (char *) malloc(BUFSZ);
    if (p == 0)
    {
	fprintf(stderr, "%s: can't malloc space for status\n", argv[0]);
	exit(1);
    }
    err = std_status(&cap, p, BUFSZ, &size);
    if (err == STD_OVERFLOW)
    {
	p = (char *) realloc(p, (size_t) size);
	if (p == 0)
	{
	    fprintf(stderr, "%s: can't realloc space for status\n", argv[0]);
	    exit(1);
	}
	err = std_status(&cap, p, size, &size);
    }
    if (err != STD_OK)
    {
	fprintf(stderr, "%s: std_status failed: %s\n", argv[0], err_why(err));
	exit(1);
    }
    (void) fwrite(p, 1, size, stdout);
    if (size > 0 && p[size-1] != '\n')
    	putchar('\n');
    exit(0);
}
