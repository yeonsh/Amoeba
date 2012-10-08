/*	@(#)t2.c	1.3	96/02/27 10:54:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "thread.h"
#include "module/syscall.h"
#include "module/mutex.h"

#define NTHREADS	10
#define LOOP		4000000

mutex pr_mu;
mutex sink;
int worker_id = 0;
int ready = 0;
long max;
int Worker[NTHREADS];
unsigned long TimeUsed[NTHREADS];

void
worker(p, sz)
char *p;
int sz;
{
	int id;
	int j;
	long old;
	unsigned long start;

	start = sys_milli();

	mu_lock(&pr_mu);
	id = worker_id++;
	mu_unlock(&pr_mu);

	thread_set_priority((long) 1, &old);
	if (old != max)
		printf("HELP: worker %d started with priority %d\n", id,
			(int) old);
	mu_lock(&pr_mu);
	printf("Worker %d ready for action\n", id);
	mu_unlock(&pr_mu);

	mu_lock(&sink); /* wait for other threads to catch up */
	mu_unlock(&sink);

	for (j = 0; j < LOOP; j++)
		Worker[id] += 1;

	mu_lock(&pr_mu);
	ready++;
	mu_unlock(&pr_mu);

	TimeUsed[id] = sys_milli() - start;
	thread_exit();
}


main(argc, argv)
int argc;
char *argv[];
{
	int nthreads;
	long old;
	int j;

	if (argc == 1) {
		nthreads = NTHREADS;
	} else {
		nthreads = atoi(argv[1]);
		if (nthreads < 1 || nthreads > NTHREADS) {
			printf("illegal nr. of threads (max %d)\n", NTHREADS);
			exit(1);
		}
	}
		
	mu_init(&pr_mu);
	mu_init(&sink);

	thread_get_max_priority(&max);
	printf("priorities from 0 to %d\n", (int) max);

	thread_enable_preemption();

	/* Set thread priority to max and then test if the old value
	 * returned is correct.
	 */
	thread_set_priority(max, &old);

	if (old != 0)
		printf("HELP: initial priority not zero (%d)\n", (int) old);

	thread_set_priority(max, &old);

	if (old != max)
		printf("HELP: priority not set to %d but %d\n", (int) max,
			(int) old);
	
	/* block the workers until we are ready */
	mu_lock(&sink);

	for (j = 0; j < nthreads; j++) {
		if (thread_newthread(worker, 4096, (char *) 0, 0) == 0) {
			printf("thread_newthread() failed\n");
			exit(13);
		}
	}

	/* Give the other threads are chance to run and set their priority
	 * before we release the sink plug :-)
	 */
	mu_lock(&pr_mu);
	printf("main finished creating threads\n");
	mu_unlock(&pr_mu);

	sleep(2);
	/* Let the workers run */
	mu_unlock(&sink);

	/* Give this thread a higher priority in order to reduce the influence
	 * of the printf()s on the scheduling of the other threads.
	 */
	thread_set_priority((long) 2, &old);
	while (ready != nthreads) {
		for (j = 0; j < nthreads; j++)
			printf("%d ", Worker[j]);
		printf("\n");
		sleep(1);
	}
	for (j = 0; j < nthreads; j++)
		printf("%d ", Worker[j]);
	printf("\n\n");

	printf("\nTime used:\n");
	for (j = 0; j < nthreads; j++)
		printf("%ld ", TimeUsed[j]);
	printf("\n");

	thread_exit();
}
