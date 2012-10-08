/*	@(#)sysmilli.c	1.3	96/02/27 10:57:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include "module/syscall.h"

#define	CNT	1000000

main()
{
    int i;
    unsigned long t1, t2;

    t1 = sys_milli();
    i = CNT;
    while (--i != 0)
	t2 = sys_milli();
    printf("%d sys_milli calls took %d ms\n", CNT, t2 - t1);
    exit(0);
    /*NOTREACHED*/
}
