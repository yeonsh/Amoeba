/*	@(#)format.c	1.3	96/02/27 13:18:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <stdio.h>
#include <stdlib.h>
#include <module/disk.h>
#include <module/name.h>


main(argc, argv)
char *argv[];
{
    capability cap;
    errstat err;

    if (argc != 2)
    {
	fprintf(stderr, "Usage: %s vdisk\n", argv[0]); 
	exit(1);
    }

    if ((err = name_lookup(argv[1], &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: can't get capability for %s: %s\n",
					argv[0], argv[1], err_why(err));
	exit(1);
    }

    err = disk_control(&cap, DSC_FORMAT, NILBUF, 0);
    if (err != STD_OK)
    {
	fprintf(stderr, "%s: disk_control error: %s\n", argv[0], err_why(err));
	exit(1);
    }
    exit(0);
}
