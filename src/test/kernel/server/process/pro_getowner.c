/*	@(#)pro_getowner.c	1.3	96/02/27 10:55:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	PRO_GETOWNER TEST
*/

#include <stdlib.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "module/proc.h"
#include "module/ar.h"

main(argc, argv)
int	argc;
char *	argv[];
{
    capability	process;
    capability	cur_owner;
    errstat	err;


    if (argc != 2)
	exit(usage(argv[0]));

/* get capability for process server */
    if ((err = name_lookup(argv[1], &process)) != STD_OK)
    {
	printf("%s: lookup of '%s' failed (%s)\n",
		argv[0], argv[1], err_why(err));
	exit(1);
    }

/* make the call */
    if ((err = pro_getowner(&process, &cur_owner)) != STD_OK)
    {
	printf("%s: get failed for '%s' (%s)\n",
		argv[0], argv[1], err_why(err));
	exit(1);
    }
    printf("Owner of '%s' has capability %s\n", argv[1], ar_cap(&cur_owner));
    exit(0);
    /*NOTREACHED*/
}


int
usage(progname)
char *	progname;
{
    printf("Usage: %s process-capability\n", progname);
    return 1;
}
