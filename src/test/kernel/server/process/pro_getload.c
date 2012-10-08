/*	@(#)pro_getload.c	1.3	96/02/27 10:55:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
/*
**	PRO_GETLOAD TEST
*/

#include <stdlib.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "module/proc.h"

main(argc, argv)
int	argc;
char *	argv[];
{
    capability	cap;
    errstat	err;
    long	ips;
    long	loadav;
    long	mfree;

    if (argc != 2)
	exit(usage(argv[0]));

/* get capability for process server */
    if ((err = name_lookup(argv[1], &cap)) != STD_OK)
    {
	printf("%s: lookup of '%s' failed (%s)\n",
		argv[0], argv[1], err_why(err));
	exit(1);
    }

/* make the call */
    if ((err = pro_getload(&cap, &ips, &loadav, &mfree)) != STD_OK)
    {
	printf("%s: get failed for '%s' (%s)\n",
		argv[0], argv[1], err_why(err));
	exit(1);
    }
    printf("server '%s' returned\n", argv[1]);
    printf("  instructions/sec    = %d\n", ips);
    printf("  free memory (bytes) = %d\n", mfree);
    printf("  load average        = %d\n", loadav);
    exit(0);
    /*NOTREACHED*/
}


int
usage(progname)
char *	progname;
{
    printf("Usage: %s process-server-capability\n", progname);
    return 1;
}
