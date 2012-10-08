/*	@(#)stkovrfl2.c	1.2	94/04/06 17:41:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This program should crash.  It attempts to overrun its stack by
 * simply having too much on the stack. when it make a procedure call.
 * In principle it should die due to illegal memory reference.
 */

#include <stdlib.h>

#define	CHUNKSIZE	4096*8


badfunc(depth)
int depth;
{
    char stackfill[CHUNKSIZE];
    int i;

#if 0
    printf("badfunc %d still going\n", depth);
#endif
    stackfill[CHUNKSIZE - 1] = 100;
    badfunc(++depth);
    for (i = 0 ; i < CHUNKSIZE; i++)
	stackfill[i] = i;
}

/*ARGSUSED*/
main(argc, argv)
int argc;
char * argv[];
{
    printf("Expect stack overflow (Memory fault)\n");
    badfunc(1);
    exit(0);
    /*NOTREACHED*/
}
