/*	@(#)jobset.c	1.4	96/02/27 13:11:12 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*

JOBSETS
-------

An easy interface to schedule groups of  concurrent jobs.  A 'job' is
defined as a void function with a single pointer argument, which runs
to completion in a finite time.  A 'jobset' is a data structure
containing a queue of jobs to be executed, and a collection of worker
threads,  which repeatedly take jobs from the work queue and execute
them.  When the queue is empty, workers remain idle, blocked on a
semaphore, waiting for more work, until the jobset is destroyed.  Jobs
are taken from the queue in FIFO order, but needn't complete in that
order.

	struct jobset *js;
		pointer to jobset data structure
	js = js_create(stacksize, nworkers)
		creates a new jobset
	js_moreworkers(js, n)
		add n more workers
	js_wait(js)
		wait until all workers are idle
	js_destroy(js)
		destroy the jobset and and its workers;
		implies js_sync(js)
	js_addjob(js, func, data)
		adds execution of func(data) to the queue



Semantical differences with threads: jobs should never call thread_exit or
exitthread; the data pointer passed to a job is not freed by the jobs
package; jobs don't have glocal data; all jobs (in a given jobset) have
the same stack size; not all jobs are runnable concurrently; jobs can be
waited for collectively.

Jobs are cheaper than threads: on a VaxStation 2000, scheduling a thread
(to execute a null function) and running it to completion takes about
3.2 ms, while the same for a job takes only .57 ms: .26 ms in js_create,
.31 ms to run it.

Author: Guido van Rossum, CWI

*/

#include <amtools.h>
#include <semaphore.h>
#include <thread.h>

/* Job data structure */
struct job {
	struct job *j_next;		/* Next job in queue */
	void (*j_func)();		/* Function to be called */
	char *j_data;			/* Argument to that function */
};

/* Jobset data structure */
struct jobset {
	mutex js_mu;			/* Protects remaining fields */
	struct job *js_head, *js_tail;	/* Queue head and tail */
	semaphore js_inqueue;		/* Queue size */
	int js_added;			/* Number of jobs added */
	semaphore js_done;		/* Number of jobs completed */
	
	int js_nworkers;		/* Number of workers */
	int js_stacksize;		/* Worker stack size */
	int js_shutdown;		/* Set to shut down workers */
};

/* Debugging macros */
#ifdef NDEBUG
#define CHECK(expr, msg) /*empty*/
#define JSCHECK(js, msg) /*empty*/
#else
#define CHECK(expr, msg) \
	if (!(expr)) { \
		fprintf(stderr, "jobs package: %s\n", msg); \
		abort(); \
	}
#define JSCHECK(js, msg) CHECK((js) != NULL, msg)
#endif

/* Create a new jobset */

struct jobset *
js_create(stacksize, nworkers)
	int stacksize;
	int nworkers;
{
	register struct jobset *js;
	
	js = (struct jobset *)malloc(sizeof(struct jobset));
	if (js == NULL)
		return NULL;
	
	js->js_stacksize = stacksize;
	js->js_shutdown = 0;
	js->js_nworkers = 0;
	mu_init(&js->js_mu);
	js->js_head = js->js_tail = NULL;
	sema_init(&js->js_inqueue, 0);
	js->js_added = 0;
	sema_init(&js->js_done, 0);
	
	if (js_moreworkers(js, nworkers) == 0) {
		free(js);
		return NULL;
	}
	
	return js;
}


/* Add a job to the job queue.
   Return a pointer referencing the job, or NULL if something went
   wrong. */

int
js_addjob(js, func, data)
	register struct jobset *js;
	void (*func)();
	char *data;
{
	register struct job *j;
	
	JSCHECK(js, "bad jobset passed to js_addjob");
	CHECK(func != NULL, "NULL function passed to js_addjob");
	
	j = (struct job *)malloc(sizeof(struct job));
	if (j == NULL)
		return STD_NOSPACE;
	
	j->j_next = NULL;
	j->j_func = func;
	j->j_data = data;
	
	mu_lock(&js->js_mu);
	if (js->js_tail == NULL) {
		CHECK(js->js_head == NULL, "unexpected NULL tail in js_addjob");
		js->js_tail = js->js_head = j;
	}
	else {
		CHECK(js->js_head != NULL, "unexpected NULL head in js_addjob");
		js->js_tail->j_next = j;
		js->js_tail = j;
	}
	++js->js_added;
	mu_unlock(&js->js_mu);
	
	sema_up(&js->js_inqueue);
	return STD_OK;
}

/* Wait until all workers are idle and the queue is empty.
   Tricky: jobs started while we are waiting must also be complete! */

js_wait(js)
	register struct jobset *js;
{
	register int n;
	
	JSCHECK(js, "bad jobset passed to js_wait");
	for (;;) {
		mu_lock(&js->js_mu);
		n = js->js_added;
		js->js_added = 0;
		mu_unlock(&js->js_mu);
		if (n <= 0)
			break;
		while (n > 0)
			n -= sema_mdown(&js->js_done, n);
	}
}

/* Destroy the jobset and its workers.  Implies js_wait(js). */

js_destroy(js)
	register struct jobset *js;
{
	register int n;
	
	js_wait(js);
	js->js_shutdown = 1;
	n = js->js_nworkers;
	sema_mup(&js->js_inqueue, n);
	while (n > 0)
		n -= sema_mdown(&js->js_done, n);
	free(js);
}

/* A worker thread.
   Loop forever, taking jobs from the queue and executing them. */

/*ARGSUSED*/
static void
_js_worker(arg, sz)
	char *arg;
	int sz;		/* unused */
{
	register struct jobset *js = *(struct jobset **)arg;
	register struct job *j;
	
	for (;;) {
		sema_down(&js->js_inqueue); /* Block until work to do */
		
		if (js->js_shutdown) {
			sema_up(&js->js_done);
			break;
		}
		
		mu_lock(&js->js_mu);
		j = js->js_head;
		js->js_head = j->j_next;
		if (js->js_head == NULL)
			js->js_tail = NULL;
		mu_unlock(&js->js_mu);
		
		j->j_func(j->j_data); /* Run the job */
		
		sema_up(&js->js_done);
		free(j);
	}
}

/* Create n more worker threads.
   Returns the number of workers actually created. */

int
js_moreworkers(js, n)
	struct jobset *js;
	register int n;
{
	register struct jobset **pjs;
	register int nstarted = 0;
	
	while (--n >= 0) {
		pjs = (struct jobset **)malloc(sizeof(struct jobset *));
		if (pjs == NULL)
			break;
		*pjs = js;
		if (!thread_newthread(_js_worker,
				js->js_stacksize, (char *)pjs, 0)) {
			free(pjs);
			break;
		}
		++nstarted;
	}
	js->js_nworkers += nstarted;
	return nstarted;
}
