/*	@(#)fromb.c	1.4	94/04/07 15:03:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** fromb
**	Read the bullet file 'bulletfile' and write it on stdout.
**	If offset is specified start reading at byte 'offset' in the file.
**	If size is specified read a maximum of 'size' bytes.
**	It reads the file in 30000 byte chunks.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/name.h"

#include "stdio.h"
#include "stdlib.h"

#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#define	BFSZ		30000

extern int	optind;
extern char *	optarg;

char		buff[BFSZ];	/* transaction buffer for b_read */

void		usage();

main(argc, argv)
int argc;
char **argv;
{
    capability	cap;
    errstat	err;
    b_fsize	nread;
    b_fsize	size; 	/* number of bytes to read from the file */
    b_fsize	usize;	/* number of bytes user specified in -s option */
    b_fsize	offset; /* where to begin reading within the file */
    int		opt;
    char *	bulletfile;
    
/* parse the command line arguments */
    usize = -1;
    offset = 0;	/* default offset is start of file */
    while ((opt = getopt(argc, argv, "o:s:")) != EOF)
	switch (opt)
	{
	case 'o':
	    offset = atol(optarg);
	    break;
	case 's':
	    usize = atol(optarg);
	    break;
	default:
	    usage(argv[0]);
	    /*NOTREACHED*/
	}

    if (optind != argc - 1)
    {
	usage(argv[0]);
	/*NOTREACHED*/
    }
    bulletfile = argv[optind];

/* get capability for bullet file */
    if ((err = name_lookup(bulletfile, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					    argv[0], bulletfile, err_why(err));
	exit(1);
	/*NOTREACHED*/
    }

    if ((err = b_size(&cap, &size)) != STD_OK)
    {
	fprintf(stderr, "%s: Cannot get size of %s: %s\n",
					argv[0], bulletfile, err_why(err));
	exit(1);
	/*NOTREACHED*/
    }
    if (size < 0)
    {
	fprintf(stderr, "%s: Got negative filesize for %s\n",
						    argv[0], bulletfile);
	exit(1);
	/*NOTREACHED*/
    }
    if (offset > size)
    {
	fprintf(stderr, "%s: offset %ld is greater than the file size %ld\n",
						    argv[0], offset, size);
	exit(1);
	/*NOTREACHED*/
    }

/*
** if the number of bytes to read is not specified we read the whole file
** otherwise we make sure that the size specified is no bigger than the
** file
*/
    if (usize != -1)	/* user specified a byte count */
	if (offset + usize > size)  /* the user asked for more than there is */
	    size -= offset;
	else
	    size = usize;

    while (size > 0)
    {
	b_fsize	nbytes;

	nbytes = MIN(size, BFSZ);
	if ((err = b_read(&cap, offset, buff, nbytes, &nread)) != STD_OK)
	{
	    fprintf(stderr,
			"%s: Bullet read failed: %s\n", argv[0], err_why(err));
	    exit(1);
	    /*NOTREACHED*/
	}
	if (fwrite(buff, 1, (size_t) nread, stdout) != (int) nread)
	{
	    fprintf(stderr, "%s: fwrite error\n", argv[0]);
	    exit(1);
	    /*NOTREACHED*/
	}
	size -= nread;
	offset += nread;
    }
    exit(0);
    /*NOTREACHED*/
}


void
usage(prog)
char *	prog;
{
    fprintf(stderr, "Usage: %s [-s size] [-o offset] bulletfile\n", prog);
    exit(1);
    /*NOTREACHED*/
}
