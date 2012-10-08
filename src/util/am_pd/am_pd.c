/*	@(#)am_pd.c	1.3	96/02/27 13:05:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * am_pd
 *	Work out if the file specified is an Amoeba binary.
 *	The original shell-script for this was too slow under Amoeba due
 *	to the many pipes.  Note that this has to work under UNIX on UNIX
 *	files so it can't just do a name_lookup and a pd_read (alas).
 *	Therefore, just like the script, this can be fooled by a data file
 *	that begins with an architecture name (such as mc68000) followed by
 *	sufficient NULL bytes.
 *
 * Author: Gregory J. Sharp, Nov 1993
 */

#include "stdio.h"
#include "string.h"
#include "amoeba.h"
#include "module/proc.h"
#include "fcntl.h"


char Known_arch[][8] = {
	"i80386",
	"mc68000",
	"sparc",
	""
};


main(argc, argv)
int argc;
char * argv[];
{
    char	buf[ARCHSIZE];
    int		fd;
    int		i;

    if (argc != 2 && argc != 3)
    {
	fprintf(stderr, "Usage: %s file [architecture]\n", argv[0]);
	exit(1);
    }

    if ((fd = open(argv[1], O_RDONLY)) < 0)
    {
	fprintf(stderr, "%s: cannot open file ", argv[0]);
	perror(argv[1]);
	exit(1);
    }
    
    /* The architecture info in a pd is in the first ARCHSIZE bytes */
    if (read(fd, buf, ARCHSIZE) != ARCHSIZE)
	exit(1);

    if (argc == 3)
    {
	/* Rewrite the Known_arch list to have just the one entry. */
	(void) strncpy(Known_arch[0], argv[2], ARCHSIZE);
	Known_arch[1][0] = '\0';
    }

    /* We compare with known architectures list */
    for (i = 0; Known_arch[i][0] != '\0'; i++)
    {
	if (memcmp(Known_arch[i], buf, ARCHSIZE) == 0)
	    exit(0);
    }
    exit(1);
}
