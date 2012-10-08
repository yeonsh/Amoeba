/*	@(#)stackovf.c	1.2	94/04/06 17:41:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "thread.h"
#include <module/mutex.h>

static mutex mu;

sum(n) {

	if (n==0)
		return 0;
	return sum(n-1)+n;
}

/*ARGSUSED*/
void
go_overflow(param, size)
char *param;
int   size;
{
	int i;

	for (i=1;i<1000;i++) {
		printf("S%d = %d\n", i, sum(i));
	}
	mu_unlock(&mu);
}

main() {
	printf("Expect stack overflow (Memory fault)\n");
	mu_init(&mu);
	mu_lock(&mu);
	(void) thread_newthread(go_overflow, 4000, (char *) 0, 0);
	mu_lock(&mu);
	printf("No stack overflow?!\n");
	return 1;
}
