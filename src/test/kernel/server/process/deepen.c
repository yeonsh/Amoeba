/*	@(#)deepen.c	1.3	96/02/27 10:55:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "module/syscall.h"

int
descend(level)
int level;
{
	int res;

	if (level!=0)
		res = 1 + descend(level-1);
	else
		res = 0;
	sys_null();
	return res;
}

main() {
	int i, res;

	for (i=0; i<100; i++) {
		if ((res = descend(i)) != i) {
			printf("descend(%d) is %d, not %d!\n", i, res, i);
			exit(1);
		}
	}
	printf("Stack OK\n");
	exit(0);
	/*NOTREACHED*/
}
