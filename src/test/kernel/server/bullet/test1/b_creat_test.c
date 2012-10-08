/*	@(#)b_creat_test.c	1.2	94/04/06 17:36:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	Time how long it takes to create and then delete a file
**	of the given size.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/name.h"

#include "stdio.h"
#include "stdlib.h"

void		usage();

main(argc, argv)
int argc;
char **argv;
{
    register int	i;
    capability	cap;
    errstat	err;
    b_fsize	size; 	/* number of bytes in file to be created */
    char *	bulletsvr;
    long	begtime, endtime;
    char *	buf;
    int		count;
    int		safety;
    
/* parse the command line arguments */
    if (argc != 5)
	usage(argv[0]);

    bulletsvr = argv[1];
    size = atol(argv[2]) * 1024;
    count = atoi(argv[3]);
    safety = atoi(argv[4]);

/* get capability for bullet server */
    if ((err = name_lookup(bulletsvr, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					    argv[0], bulletsvr, err_why(err));
	exit(1);
    }

    if (size < 0)
    {
	fprintf(stderr, "%s: illegal size (%ld)\n",
						    argv[0], size);
	exit(1);
    }

    if ((buf = malloc((unsigned) size)) == 0)
    {
	fprintf(stderr, "malloc failed for %ld bytes\n", size);
	exit(1);
    }

    if (safety)
	safety = BS_COMMIT | BS_SAFETY;
    else
	safety = BS_COMMIT;

    printf("b_creating a file of %ld bytes %d times with safety %d\n",
		size, count, safety);
    i = count;
    begtime = time();

    while (--i >= 0)
    {
	capability newcap;

	if ((err = b_create(&cap, buf, size, safety, &newcap)) != STD_OK)
	{
	    fprintf(stderr, "bcreate failed (%s)\n", err_why(err));
	    exit(1);
	}
	if ((err = std_destroy(&newcap)) != STD_OK)
	{
	    fprintf(stderr, "std_destroy failed (%s)\n", err_why(err));
	    exit(1);
	}
    }

    endtime = time();
    endtime -= begtime;

    printf("Total bytes created = %ld\n", count*size);
    printf("Total time taken = %ld seconds\n", endtime);
    exit(0);
}


void
usage(prog)
char *	prog;
{
    fprintf(stderr, "Usage: %s bullet_server size count\n", prog);
    exit(1);
}
