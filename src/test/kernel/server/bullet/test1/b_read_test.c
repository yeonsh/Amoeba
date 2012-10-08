/*	@(#)b_read_test.c	1.2	94/04/06 17:36:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_read_test
**	Read the bullet file 'bulletfile' count times and time how long it took
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/name.h"

#include "stdio.h"
#include "stdlib.h"

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

extern int	optind;
extern char *	optarg;

void		usage();

main(argc, argv)
int argc;
char **argv;
{
    register b_fsize	offset; /* where to begin reading within the file */
    register int	i;
    register int	total;
    capability	cap;
    errstat	err;
    b_fsize	nread;
    b_fsize	size; 	/* number of bytes to read from the file */
    char *	bulletfile;
    int		count;
    long	begtime, endtime;
    char *	buf;
    
/* parse the command line arguments */
    if (argc != 3)
	usage(argv[0]);

    bulletfile = argv[1];
    count = atoi(argv[2]);

/* get capability for bullet file */
    if ((err = name_lookup(bulletfile, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					    argv[0], bulletfile, err_why(err));
	exit(1);
    }

    if ((err = b_size(&cap, &size)) != STD_OK)
    {
	fprintf(stderr, "%s: Cannot get size of %s: %s\n",
					argv[0], bulletfile, err_why(err));
	exit(1);
    }
    if (size < 0)
    {
	fprintf(stderr, "%s: Got negative filesize for %s\n",
						    argv[0], bulletfile);
	exit(1);
    }
    if ((buf = malloc(size)) == 0)
    {
	fprintf(stderr, "%s: malloc of size %ld bytes failed\n", argv[0], size);
	exit(1);
    }

/*
** if the number of bytes to read is not specified we read the whole file
** otherwise we make sure that the size specified is no bigger than the
** file
*/
    printf("b_reading file of %ld bytes %d times\n", size, count);
    total = 0;
    i = count;

    begtime = time();

    while (--i >= 0)
    {
	if ((err = b_read(&cap, 0L, buf, size, &nread)) != STD_OK)
	{
	    fprintf(stderr,
			"%s: Bullet read failed: %s\n", argv[0], err_why(err));
	    exit(1);
	}
    }

    endtime = time();
    endtime -= begtime;


    printf("Total bytes read = %ld\n", size * count);
    printf("Total time taken = %ld seconds\n", endtime);
    printf("Average time to read file of %ld bytes = %ld seconds\n", size, endtime/count);
    exit(0);
}


void
usage(prog)
char *	prog;
{
    fprintf(stderr, "Usage: %s bulletfile count\n", prog);
    exit(1);
}
