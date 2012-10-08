/*	@(#)thrdsw.c	1.3	96/02/27 10:57:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <stdlib.h>
#include "amoeba.h"
#include "thread.h"
#include "module/mutex.h"
#include "module/syscall.h"


mutex		starter;
unsigned long	t1, t2;
long		num_switches;
long		max_prio;
#define	CNT	1000000

mutex		mu_print;
#define	PRINTF(a)	mu_lock(&mu_print); printf a; mu_unlock(&mu_print);

static void
doswitch()
{
#ifdef PREEMPTIVE
    long oldprio;

    thread_set_priority(max_prio, &oldprio);
    /* give other threads a chance to increase their priority */
    sleep(1);
#endif
    while (--num_switches > 0)
	threadswitch();
}

/*ARGSUSED*/
void
tswt(param, sz)
char *	param;
int	sz;
{
    mu_lock(&starter);
    mu_unlock(&starter);
    doswitch();
    thread_exit();
}


main()
{
    mu_init(&starter);
    mu_init(&mu_print);
    mu_lock(&starter);
#ifdef PREEMPTIVE
    thread_get_max_priority(&max_prio);
    thread_enable_preemption();
#endif
    if (!thread_newthread(tswt, 8192, (char *) 0, 0))
    {
	printf("thread_newthread 1 failed\n");
	exit(1);
    }
    if (!thread_newthread(tswt, 8192, (char *) 0, 0))
    {
	printf("thread_newthread 1 failed\n");
	exit(1);
    }
    num_switches = CNT;
    t1 = sys_milli();
    mu_unlock(&starter); /* let the two threads go at it */
    doswitch();
    t2 = sys_milli();
    printf("%d switches took %d ms\n", CNT, t2 - t1);
    exit(0);
    /*NOTREACHED*/
}
