/*	@(#)std_ntouch.c	1.2	94/04/07 16:08:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This program attempts to touch 'n' objects.  All must have the same server port.
 * No attempt is made to deal capability sets since these will vanish by the time
 * this program is released.
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
    errstat	err;
    capability	cap;
    int		status;	/* count of numer of errors detected */
    int		i;
    int		n;	/* # caps in privbuf */
    int		done;
    private *	privbuf;

    if (argc < 2)
    {
	fprintf(stderr, "Usage: %s capability ...\n", argv[0]);
	exit(-1);
    }

    privbuf = (private *) malloc(sizeof (private) * (argc - 1));
    if (privbuf == 0)
    {
	fprintf(stderr, "%s: cannot malloc buffer for %d caps\n", argv[0], argc - 1);
	exit(-1);
    }

    /*
     * We gather up the capabilities and stick the private parts in privbuf.
     */
    n = 0;
    for (i = 1; i < argc; i++)
    {
	if ((err = name_lookup(argv[i], &cap)) != STD_OK)
	{
	    fprintf(stderr, "%s: lookup of %s failed: (%s)\n",
						argv[0], argv[i], err_why(err));
	    status++;
	}
	else
	{
	    privbuf[n++] = cap.cap_priv;
	}
    }
    /*
     * Now we try to touch them all at once.
     */
    (void) timeout((interval) 5000);
    if ((err = std_ntouch(&cap.cap_port, n, privbuf, &done)) != STD_OK)
    {
	done = n - done; /* make it an error count */
	fprintf(stderr, "%s: ntouch had %d %s (%s)\n", argv[0], done,
				(done == 1) ? "error" : "errors", err_why(err));
	status += done;
    }

    exit(status);
}
