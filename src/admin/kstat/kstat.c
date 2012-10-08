/*	@(#)kstat.c	1.4	94/04/06 11:47:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	KSTAT
**
** This program sends a request to the specified amoeba kernel to return a
** preformatted copy of the specified system table.  If more than one table
** is specified a separate request will be sent for each table.
**
** Author: Greg Sharp
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/host.h"
#include "module/systask.h"

#include "stdio.h"
#include "stdlib.h"

#define KSTAT_BUFSZ	30000

static char	Reqbuf[KSTAT_BUFSZ];


void
usage(progname)
char *	progname;
{
    fprintf(stderr, "Usage: %s -flags machine\n", progname);
    fprintf(stderr,
	"Use: %s -? machine\nto get a list of flags supported by 'machine'.\n",
								progname);
    exit(1);
}


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat		error;
    register char *	flags;
    char *		machine;
    capability		mcap;
    register char *	p;
    char *		progname;
    int			result;

    progname = argv[0];

    if (argc != 3)
	usage(progname);

    flags = argv[1];
    machine = argv[2];

/* see if machine exists */
    if ((error = syscap_lookup(machine, &mcap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					    progname, machine, err_why(error));
	exit(1);
    }

/* For each flag send a request and print the result */
    if (*flags == '-')
	flags++;
    if (*flags == '\0')	/* no flags were given */
	usage(progname);

/* see if machine exists */
    for ( ; *flags; flags++)
    {
	error = sys_kstat(&mcap, *flags, Reqbuf, KSTAT_BUFSZ, &result);
	if (error == STD_OK)
	{
	    p = Reqbuf;
	    while (--result >= 0)
		putchar(*p++);
	    putchar('\n');
	}
	else
	    fprintf(stderr, "%s: failed for flag %c: %s\n",
					    argv[0], *flags, err_why(error));
    }
    exit(0);
}
