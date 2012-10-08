/*	@(#)std_touch.c	1.3	94/04/07 16:09:41 */
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
    int		status;	/* count of numer of errors detected */
    int		i;	/* loop counter */

    (void)timeout((interval)10000);
    if (argc < 2)
    {
	fprintf(stderr, "Usage: %s servercap ...\n", argv[0]);
	exit(1);
    }
    status = 0;
    for (i = 1; i < argc; i++)
    {
	if ((err = name_lookup(argv[i], &cap)) != STD_OK)
	{
	    fprintf(stderr, "%s: cannot find %s (%s)\n",
						argv[0], argv[i], err_why(err));
	    status++;
	    continue;
	}
	(void)timeout((interval)5000);
	if ((err = std_touch(&cap)) != STD_OK)
	{
	    fprintf(stderr, "%s: cannot touch %s (%s)\n",
						argv[0], argv[i], err_why(err));
	    status++;
	    continue;
	}
    }
    exit(status);
}
