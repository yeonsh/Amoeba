/*	@(#)std_age.c	1.3	94/04/06 12:00:09 */
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

main(argc, argv)
int	argc;
char *	argv[];
{
    capability	cap;
    errstat	err;

    if (argc != 2)
    {
	fprintf(stderr, "Usage: %s servercap\n", argv[0]);
	exit(1);
    }
    (void)timeout((interval)10000);
    if ((err = name_lookup(argv[1], &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
						argv[0], argv[1], err_why(err));
	exit(1);
    }
    if ((err = std_age(&cap)) != STD_OK)
    {
	fprintf(stderr, "%s: failed: %s\n", argv[0], err_why(err));
	exit(1);
    }
    exit(0);
}
