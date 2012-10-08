/*	@(#)bdkcomp.c	1.3	94/04/06 11:41:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** BDKCOMP
**
**	This program sends the specified bullet server a disk compaction
**	command.  This should eliminate any disk fragmentation
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/name.h"
#include "bullet/bullet.h"
#include "stdio.h"
#include "stdlib.h"

main(argc, argv)
int	argc;
char *	argv[];
{
    errstat	err;
    capability	bulletsvr;

    if (argc != 2)
    {
	fprintf(stderr, "Usage: %s bullet-server\n", argv[0]);
	exit(1);
    }

    if ((err = name_lookup(argv[1], &bulletsvr)) != STD_OK)
    {
	fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
						argv[0], argv[1], err_why(err));
	exit(1);
    }

    if ((err = b_disk_compact(&bulletsvr)) != STD_OK)
    {
	fprintf(stderr, "%s: disk compaction failed: %s\n",
							argv[0], err_why(err));
	exit(1);
    }

    exit(0);
}
