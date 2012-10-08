/*	@(#)std_destroy.c	1.3	94/04/07 16:08:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "server/soap/soap.h"

#include "stdio.h"
#include "stdlib.h"

main(argc, argv)
int	argc;
char **	argv;
{
    capset	cs;
    errstat	err;
    register	i, n;
    int count;
    register	status;
    interval savetimeout;

    if (argc < 2)
    {
	fprintf(stderr, "Usage: %s file ...\n", argv[0]);
	exit(1);
    }
    (void)timeout((interval)10000);
    status = 0;
    for (i = 1; i < argc; i++)
    {
	if ((err = sp_lookup(SP_DEFAULT, argv[i], &cs)) != STD_OK)
	{
	    fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
						argv[0], argv[i], err_why(err));
	    status++;
	    continue;
	}

	/* Destroy all the objects specified by the capset: */

	/* Count the number of capabilities: */
	for (n = count = 0; n < cs.cs_final; n++)
		if (cs.cs_suite[n].s_current)
			count++;

	savetimeout = timeout((interval) 5000);
	err = cs_purge(&cs);
	(void) timeout(savetimeout);

	if (err != STD_OK) {
	    int newcount;

	    /* Re-count the number of capabilities: */
	    for (n = newcount = 0; n < cs.cs_final; n++)
		if (cs.cs_suite[n].s_current)
		    newcount++;

	    if (newcount == count)
		fprintf(stderr, "%s: can't destroy %s (%s)\n",
					    argv[0], argv[i], err_why(err));
	    else
		fprintf(stderr,
			"%s: Warning: not all replicas of %s destroyed (%s)\n",
					    argv[0], argv[i], err_why(err));
	    status++;
	}
    }
    exit(status);
}
