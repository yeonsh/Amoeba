/*	@(#)pro_setowner.c	1.3	96/02/27 10:56:01 */
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

main(argc, argv)
int	argc;
char *	argv[];
{
    capability	process;
    capability	new_owner;
    errstat	err;

    if (argc != 3)
	exit(usage(argv[0]));

/* get capability for process server */
    if ((err = name_lookup(argv[1], &process)) != STD_OK)
    {
	printf("%s: lookup of '%s' failed (%s)\n",
		argv[0], argv[1], err_why(err));
	exit(1);
    }

/* get capability for new owner */
    if ((err = name_lookup(argv[2], &new_owner)) != STD_OK)
    {
	printf("%s: lookup of '%s' failed (%s)\n",
		argv[0], argv[2], err_why(err));
	exit(1);
    }

/* make the call */
    if ((err = pro_setowner(&process, &new_owner)) != STD_OK)
    {
	printf("%s: set failed for '%s' (%s)\n",
		argv[0], argv[1], err_why(err));
	exit(1);
    }
    printf("Owner of '%s' successfully set to '%s'.\n", argv[1], argv[2]);
    exit(0);
    /*NOTREACHED*/
}


int
usage(progname)
char *	progname;
{
    printf("Usage: %s process-capability new-owner-capability\n", progname);
    return 1;
}
