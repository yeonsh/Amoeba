/*	@(#)sys_null.c	1.3	96/02/27 10:57:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * sys_null.c
 *
 * Measure the number of null system calls you can make within
 * one second, and the amount of time it takes to make one.
 */
#include <amoeba.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <module/syscall.h>

#define	NCOUNT	1000000

main(argc, argv)
    int argc;
    char **argv;
{
    int32 start, finish, count;
    register uint32 i;

    count = NCOUNT;
    if (argc == 2 && (count = atoi(argv[1])) < 0) {
	fprintf(stderr, "Usage: %s [count]\n", argv[0]);
	exit(1);
    }

    time(&start);

    for (i = 0; i < count; i++)
	sys_null();

    time(&finish);

    finish -= start;
    printf("%d null system calls in %d seconds\n", count, finish);
    printf("%d null system calls per second\n", count / finish);
    printf("%d micro seconds per null system call\n",
	(finish * 1000 * 1000) / count);

    exit(0);
    /*NOTREACHED*/
}
