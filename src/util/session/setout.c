/*	@(#)setout.c	1.1	96/02/27 13:14:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * sess_setout - set the stdout used by the session server.  This also
 *		 causes /dev/console to be modified to reflect the new
 *		 stdout as well.
 * Author: Gregory J. Sharp, Aug 1994
 */

#include <stdio.h>
#include <string.h>
#include "amoeba.h"
#include "stderr.h"
#include "amtools.h"

main(argc, argv)
int	argc;
char *	argv[];
{
    errstat	 err;
    capability	 cons;
    capability * svr;

    if (argc != 2)
    {
	fprintf(stderr, "Usage: %s console-cap\n", argv[0]);
	exit(1);
    }
    /* A hack for sanity's sake */
    if (strcmp(argv[1], "/dev/tty") == 0 && (svr = getcap("STDOUT")) != NULL)
	cons = *svr;
    else
    {
	if ((err = name_lookup(argv[1], &cons)) != STD_OK)
	{
	    fprintf(stderr, "%s: lookup of `%s' failed (%s)\n",
					    argv[0], argv[1], err_why(err));
	    exit(1);
	}
    }
    if ((svr = getcap("_SESSION")) == NULL)
    {
	fprintf(stderr, "%s: no session svr available\n", argv[0]);
	exit(1);
    }
    if ((err = ses_setout(svr, cons)) != STD_OK)
    {
	fprintf(stderr, "%s: setting stdout failed (%s)\n",
					    argv[0], err_why(err));
	exit(1);
    }
    exit(0);
}
