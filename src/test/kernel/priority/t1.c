/*	@(#)t1.c	1.3	96/02/27 10:54:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This program tests to see if mutexes provide priority scheduling
 * by creating two threads which wait on a blocked mutex.  They
 * have each set their thread priority to their thread number and
 * we watch to seethe order in which the threads run when the mutex is
 * released.
 */

#include "amoeba.h"
#include "stdlib.h"
#include "thread.h"
#include "module/mutex.h"

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

#define	STKSZ		(8*1024)

mutex	blocker;
mutex	print;
long	max;

void
racer(p, sz)
char *	p;
int	sz;
{
    long	num;
    long	old;

    if (sz != 4)
    {
	printf("racer: invalid size %d\n", sz);
	exit(1);
    }

    num = *(long *) p;
    if (num > max) /* some threads will have equal priority */
	thread_set_priority(max, &old);
    else
	thread_set_priority(num, &old);

    mu_lock(&print);
    printf("racer %d ready\n", num);
    mu_unlock(&print);

    mu_lock(&blocker);
    mu_lock(&print);
    printf("racer %d: got the mutex\n", num);
    mu_unlock(&print);
    mu_unlock(&blocker);
    thread_exit();
}


main(argc, argv)
int	argc;
char *	argv[];
{
    long	old;
    long	i;
    long *	p;
    long	tcnt;

    if (argc != 2)
    {
	printf("Usage: %s threadcount\n", argv[0]);
	exit(1);
    }
    tcnt = strtol(argv[1], (char **)0, 0);
    if (tcnt < 2 || tcnt > 100)
    {
	printf("%s: value %d for thread count not sensible\n", argv[0], tcnt);
	exit(1);
    }

    thread_get_max_priority(&max);
    thread_enable_preemption();		/* turn on preemptive scheduling */
    /* set thread's priority to high */
    thread_set_priority(MIN(tcnt, max), &old);

    /*
     * Create two threads which will vie for a mutex and see if the one
     * with the highest priority gets it first
     */
    mu_init(&print);
    mu_init(&blocker);
    mu_lock(&blocker);

    for (i = 0; i < tcnt; i++)
    {
	p = (long *) malloc(sizeof (long));
	if (p == 0)
	{
	    printf("malloc failed\n");
	    exit(1);
	}
	*p = tcnt - i - 1;
	if (thread_newthread(racer, STKSZ, (char *) p, 4) == 0)
	{
	    printf("thread_newthread %d failed\n", i);
	    exit(1);
	}
    }

    sleep(2);
    mu_unlock(&blocker); /* Let the racer routines compete! */

    thread_exit(); /* can't use exit or the process dies too soon */
}
