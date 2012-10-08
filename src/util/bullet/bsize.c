/*	@(#)bsize.c	1.4	94/04/07 15:03:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** bsize
**	Get the size of the given bullet files.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/name.h"
#include "stdio.h"
#include "stdlib.h"


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat	b_size();

    capability	cap;
    errstat	err;
    int		i;
    b_fsize	size;
    int		status;

    if (argc < 2)
    {
	fprintf(stderr, "Usage: %s file [file2 ...]\n", argv[0]);
	exit(1);
    }
    status = 0;
    for (i = 1; i < argc; i++)
    {
	if ((err = name_lookup(argv[i], &cap)) != STD_OK)
	{
	    fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
						argv[0], argv[i], err_why(err));
	    status++;
	    continue;
	}
	if ((err = b_size(&cap, &size)) != STD_OK)
	{
	    fprintf(stderr, "%s: Cannot get size of %s: %s\n",
						argv[0], argv[i], err_why(err));
	    status++;
	    continue;
	}
	printf("%s: %ld\n", argv[i], size);
    }
    exit(status);
    /*NOTREACHED*/
}
