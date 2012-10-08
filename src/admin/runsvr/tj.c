/*	@(#)tj.c	1.5	96/02/27 10:18:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Timed Job Sets -- A variant on jobsets, where jobs are scheduled for
   execution at a certain time from now. */

/* TO DO: remove the timer thread and let the workers do the waits. */

#include <amtools.h>
#include <semaphore.h>
#include <thread.h>
#include "tj.h"

#define TIMERSTACK	8096		/* Stack size for timer thread */

typedef unsigned long tick_t;		/* Type used for timer values */

/* Job data structure */
struct tjob {
	struct tjob *j_next;		/* Next job in queue */
	void (*j_func)();		/* Function to be called */
	char *j_data;			/* Argument to that function */
	tick_t j_when;			/* Time when to execute it */
};

/* Jobset data structure */
struct tjobset {
	mutex tj_mu;			/* Protects remaining fields */
	struct tjob *tj_head, *tj_tail;	/* Run queue head and tail */
	semaphore tj_inqueue;		/* Run queue size */
	
	struct tjob *tj_dhead, *tj_dtail;
					/* Delay queue head and tail */
	semaphore tj_indqueue;		/* Wakes up timer thread */
	
	long tj_added;			/* Number of jobs added */
	semaphore tj_done;		/* Number of jobs completed */

	int tj_nworkers;		/* Number of workers */
	int tj_stacksize;		/* Worker stack size */
	int tj_shutdown;		/* Set to shut down workers */
};

/* Debugging macros */
#ifdef NDEBUG
#define CHECK(expr, msg) /*empty*/
#define JSCHECK(tj, msg) /*empty*/
#else
#define CHECK(expr, msg) \
	if (!(expr)) { \
		fprintf(stderr, "jobs package: %s\n", msg); \
		abort(); \
	}
#define JSCHECK(tj, msg) CHECK((tj) != NULL, msg)
#endif

/* Timer thread.
   Wakes up whenever a new job is added to the delay queue,
   and whenever the job at the head of the delay queue is due to execute. */

/*ARGSUSED*/
static void
_tj_timer(arg, len)
	char *arg;
	int len;
{
	struct tjobset *tj = *(struct tjobset **)arg;
	struct tjob *j;
	/*signed!*/ long delay;
	
	/* If the delay queue is empty, sleep forever on the semaphore;
	   we'll wake up when tj_addjob has added an element at the
	   beginning and ups the semaphore.  If the delay queue is not
	   empty, use sema_trydown to wait until either the first
	   element is due or the sema is upped again.  In all cases,
	   when we find the that the first element is due, we move it to
	   the run queue. */
	
	delay = -1; /* Queue is empty initially */
	for (;;) {
		(void) sema_trydown(&tj->tj_indqueue, delay);
		if (tj->tj_shutdown)
			break;
		mu_lock(&tj->tj_mu);
		if ((j = tj->tj_dhead) == NULL)
			delay = -1;
		else {
			unsigned long now = sys_milli();

			if (now < j->j_when) {
				delay = j->j_when - now;
			} else {
				/* Remove from delay queue */
				tj->tj_dhead = j->j_next;
				if (tj->tj_dhead == NULL)
					tj->tj_dtail = NULL;
				/* Append to run queue */
				j->j_next = NULL;
				if (tj->tj_tail == NULL)
					tj->tj_head = j;
				else
					tj->tj_tail->j_next = j;
				tj->tj_tail = j;
				sema_up(&tj->tj_inqueue);
				delay = 0; /* Immediately cycle again */
			}
		}
		mu_unlock(&tj->tj_mu);
	}
	
	/* Shut down time -- free resources. */
	free((_VOIDSTAR) tj);
}

/* Create a new jobset */

struct tjobset *
tj_create(stacksize, nworkers)
	long stacksize;
	int nworkers;
{
	struct tjobset *tj, **ptj;
	
	tj = (struct tjobset *)malloc(sizeof(struct tjobset));
	if (tj == NULL)
		return NULL;
	
	mu_init(&tj->tj_mu);
	tj->tj_head = tj->tj_tail = NULL;
	sema_init(&tj->tj_inqueue, 0);
	
	tj->tj_dhead = tj->tj_dtail = NULL;
	sema_init(&tj->tj_indqueue, 0);
	
	tj->tj_added = 0;
	sema_init(&tj->tj_done, 0);
	
	tj->tj_nworkers = 0;
	tj->tj_stacksize = stacksize;
	tj->tj_shutdown = 0;
	
	ptj = (struct tjobset **)malloc(sizeof(struct tjobset *));
	if (ptj == NULL) {
		free((_VOIDSTAR)tj);
		return NULL;
	}
	*ptj = tj;
	if (!thread_newthread(_tj_timer, TIMERSTACK, (char *)ptj, 0)) {
		free((_VOIDSTAR)tj);
		free((_VOIDSTAR)ptj);
		return NULL;
	}
	
	if (tj_moreworkers(tj, nworkers) == 0) {
		tj_destroy(tj);
		return NULL;
	}
	
	return tj;
}

/* Return time left before we intend to run the job specified
 * This can be used to decide whether it is sensible to reschedule a job.
 */
errstat
tj_timeleft(tj, func, data, left)
	struct tjobset *tj;
	void (*func)();
	char *data;
	long *left;
{
	struct tjob *p;
	errstat ret = STD_NOTFOUND;
	
	mu_lock(&tj->tj_mu);
	for (p = tj->tj_dhead; p != NULL; p = p->j_next) {
		if (p->j_data == data && (func == NULL || p->j_func == func)) {
			unsigned long now = sys_milli();

			if (now < p->j_when) {
				*left = p->j_when - now;
			} else {
				*left = 0;
			}
			ret = STD_OK;
		}
	}
	mu_unlock(&tj->tj_mu);
	return ret;
}

/* Add a job to the job queue.
   Return a pointer referencing the job, or NULL if something went
   wrong. */

errstat
tj_addjob(tj, func, data, delay)
	struct tjobset *tj;
	void (*func)();
	char *data;
	long delay;
{
	struct tjob *j, *p, *q;
	tick_t when;
	
	JSCHECK(tj, "bad jobset passed to tj_addjob");
	CHECK(func != NULL, "NULL function passed to tj_addjob");
	
	j = (struct tjob *)malloc(sizeof(struct tjob));
	if (j == NULL)
		return STD_NOSPACE;
	
	j->j_next = NULL;
	j->j_func = func;
	j->j_data = data;
	
	mu_lock(&tj->tj_mu);
	j->j_when = when = sys_milli() + delay;
	if (delay > 0) { /* Insert in delayed queue, sorted by time */
		if ((p = tj->tj_dhead) == NULL)
			tj->tj_dhead = tj->tj_dtail = j;
		else if (p->j_when > when) { /* Insert before p */
			j->j_next = p;
			tj->tj_dhead = j;
		}
		else { /* Insert after p */
			while ((q = p->j_next) != NULL && q->j_when <= when)
				p = q;
			j->j_next = q;
			p->j_next = j;
			if (q == NULL)
				tj->tj_dtail = j;
		}
		if (j == tj->tj_dhead)
			sema_up(&tj->tj_indqueue); /* Wake up timer thread */
	}
	else { /* Insert in run queue */
		if (tj->tj_tail == NULL)
			tj->tj_head = j;
		else
			tj->tj_tail->j_next = j;
		tj->tj_tail = j;
		sema_up(&tj->tj_inqueue);
	}
	++tj->tj_added;
	mu_unlock(&tj->tj_mu);
	
	return STD_OK;
}

/* Remove a job from the delay queue.  Jobs are identified uniquely
   by the combination of function and data.  A NULL function matches
   any function (but NULL data doesn't match any data).  Returns STD_OK
   for success, a negative error code (STD_NOTFOUND) if not found.  Jobs
   can't be removed from the immediate execution queue (basically
   because I only use this to move jobs ahead in their queue). */

int
tj_cancel(tj, func, data)
	struct tjobset *tj;
	void (*func)();
	char *data;
{
	struct tjob *p, *q; /* q is p's predecessor, if any */
	
	mu_lock(&tj->tj_mu);
	for (p = tj->tj_dhead, q = NULL; p != NULL; q = p, p = p->j_next) {
		if (p->j_data == data &&
				(func == NULL || p->j_func == func)) {
			if (q == NULL)
				tj->tj_dhead = p->j_next;
			else
				q->j_next = p->j_next;
			if (p == tj->tj_dtail)
				tj->tj_dtail = q;
			break;
		}
	}
	mu_unlock(&tj->tj_mu);
	
	if (p == NULL)
		return STD_NOTFOUND;
	else {
		free((_VOIDSTAR)p);
		return STD_OK;
	}
}

#if 0 /* doesn't seem to be used right now */
/* Reschedule a job to a different delay.  Uses tj_cancel and tj_addjob. */

errstat
tj_reschedule(tj, func, data, newdelay)
	struct tjobset *tj;
	void (*func)();
	char *data;
	long newdelay;
{
	errstat err;
	
	if ((err = tj_cancel(tj, func, data)) == STD_OK)
		err = tj_addjob(tj, func, data, newdelay);
	return err;
}
#endif

/* Wait until all workers are idle and the queue is empty.
   Tricky: jobs started while we are waiting must also be complete! */

void
tj_wait(tj)
	struct tjobset *tj;
{
	int n;
	
	JSCHECK(tj, "bad jobset passed to tj_wait");
	for (;;) {
		mu_lock(&tj->tj_mu);
		n = tj->tj_added;
		tj->tj_added = 0;
		mu_unlock(&tj->tj_mu);
		if (n <= 0)
			break;
		while (n > 0)
			n -= sema_mdown(&tj->tj_done, n);
	}
}

/* Destroy the jobset and its workers.  Implies tj_wait(tj). */

void
tj_destroy(tj)
	struct tjobset *tj;
{
	int n;
	
	tj_wait(tj);
	tj->tj_shutdown = 1;
	n = tj->tj_nworkers;
	sema_mup(&tj->tj_inqueue, n);
	while (n > 0)
		n -= sema_mdown(&tj->tj_done, n);
	sema_up(&tj->tj_indqueue); /* Wake up timer thread for last clean-up */
}

/* A worker thread.
   Loop forever, taking jobs from the queue and executing them. */

/*ARGSUSED*/
static void
_tj_worker(arg, s)
	char *arg;
	int s;
{
	struct tjobset *tj = *(struct tjobset **)arg;
	struct tjob *j;
	
	for (;;) {
		sema_down(&tj->tj_inqueue); /* Block until work to do */
		
		if (tj->tj_shutdown) {
			sema_up(&tj->tj_done);
			break;
		}
		
		mu_lock(&tj->tj_mu);
		j = tj->tj_head;
		tj->tj_head = j->j_next;
		if (tj->tj_head == NULL)
			tj->tj_tail = NULL;
		mu_unlock(&tj->tj_mu);
		
		j->j_func(j->j_data); /* Run the job */
		
		sema_up(&tj->tj_done);
		free((_VOIDSTAR)j);
	}
}

/* Create n more worker threads.
   Returns the number of workers actually created. */

int
tj_moreworkers(tj, n)
	struct tjobset *tj;
	int n;
{
	struct tjobset **ptj;
	int i;
	
	for (i = 0; i < n; ++i) {
		ptj = (struct tjobset **)malloc(sizeof(struct tjobset *));
		if (ptj == NULL)
			break;
		*ptj = tj;
		if (!thread_newthread(_tj_worker,
				tj->tj_stacksize, (char *)ptj, 0)) {
			free((_VOIDSTAR) ptj);
			break;
		}
	}
	tj->tj_nworkers += i;
	return i;
}
