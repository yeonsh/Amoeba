/*	@(#)std_restrict.c	1.3	94/04/07 16:09:18 */
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
    capability	newcap;
    rights_bits	mask;
    errstat	err;

    (void)timeout((interval)10000);
    if (argc != 4)
    {
	fprintf(stderr, "Usage: %s oldcap mask newcap\n", argv[0]);
	exit(1);
    }
    if ((err = name_lookup(argv[1], &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
						argv[0], argv[1], err_why(err));
	exit(1);
    }
    mask = strtol(argv[2], (char **) 0, 16);
    (void)timeout((interval)5000);
    if ((err = std_restrict(&cap, mask, &newcap)) != STD_OK)
    {
	fprintf(stderr, "%s: std_restrict failed: %s\n", argv[0], err_why(err));
	exit(1);
    }
    if ((err = name_append(argv[3], &newcap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory append of '%s' failed: %s\n",
						argv[0], argv[3], err_why(err));
	exit(1);
    }
    exit(0);
}
