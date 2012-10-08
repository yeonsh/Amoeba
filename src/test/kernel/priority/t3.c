/*	@(#)t3.c	1.3	96/02/27 10:54:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "thread.h"
#include "module/mutex.h"

#define NTHREADS	10
#define LOOP		500000

mutex mu;
int worker_id = 0;
int ready = 0;
int Worker[NTHREADS];
int Prio[NTHREADS];

long max;


check_prio(id)
int id;
{
    int j;
    long old;

    if (id != 0)
	return;

    mu_lock(&mu);
    thread_set_priority(max, &old);
    for (j = 0; j < NTHREADS; j++) {
	if (Prio[j] > Prio[id]) {
	    printf("too low priority: Prio[%d] = %d, Prio[%d] = %d\n",
						    id, Prio[id], j, Prio[j]);
	    break;
	}
    }
    thread_set_priority(old, &old);
    mu_unlock(&mu);
}


void
worker(p, sz)
char *p;
int sz;
{
    int id, j;
    long old, m;

    mu_lock(&mu);
    id = worker_id++;
    mu_unlock(&mu);

    while (worker_id != NTHREADS)
	threadswitch();

    for (m = max; m >= 0; m--) {
	Prio[id] = m;
	thread_set_priority(m, &old);
	check_prio(id);
	for (j = 0; j < LOOP; j++)
	    Worker[id] += 1;
    }

    mu_lock(&mu);
    ready++;
    mu_unlock(&mu);

    thread_exit();
}


main()
{
    long old;
    int j;
    int sum;
    int last;

    mu_init(&mu);

    thread_get_max_priority(&max);
    printf("priorities from 0 to %d\n", (int) max);

    thread_enable_preemption();
    thread_set_priority(max, &old);

    if (old != 0)
	    printf("HELP: initial priority not zero (%d)\n", (int) old);

    thread_set_priority(max, &old);

    if (old != max)
	    printf("HELP: priority not set to %d but %d\n", (int) max,
		    (int) old);

    for (j = 0; j < NTHREADS; j++) {
	if (thread_newthread(worker, 4096, (char *) 0, 0) == 0) {
	    printf("thread_newthread() failed\n");
	    exit(13);
	}
    }

    thread_set_priority(0, &old);
    for ( ; ready != NTHREADS; ) {
	threadswitch();
    }
    for (j = 0; j < NTHREADS; j++)
	printf("%d ", Worker[j]);
    printf("\n");

    thread_exit();
}
