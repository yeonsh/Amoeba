/*	@(#)pool.c	1.3	96/02/27 10:18:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*	pool.c 
 *
 * manage all thread allocation
 *
 * During startup a default number of threads is created.
 * These threads are allocated as needed. They can be used for
 * any type of thread. (Request, Execute job, Age)
 * there 3 ways to alloc a thread :
 *	1) alloc_free_thread : start a free thread if one is available
 *	2) create_newthread  : create a new thread using thread_newthread
 *	3) wait_for_free_thread :
 *			if free thread available use it
 *			else 
 *				create new thread 
 *				if failed
 *					wait until thread is available
 */

#include	"saksvr.h"

static struct pool {
	mutex thread_sem;
	int thread_type;
	objnum jobnr;			/* used only for WORK type threads */
	struct pool *nthread;
} *free_threads;

static int nr_freethreads = 0;
static mutex exclude_all_threads;	/* semaphore to lock this module */

static mutex sleep_till_free_thread;	/* sleep/wakeup mutex, used when
					 * waiting for free thread
					 */
static int nr_of_waiting_req = 0;	/* nr of threads waiting for free
					 * thread (sleeping on 
					 * sleep_till_free_thread)
					 */
static struct pool *threads_for_req;	/* when a thread is freed and
					 * the waiting queue is not empty
					 * it will be appended to threads_
					 * for_req. Then sleep_until_free
					 * thread will be unlocked
					 * that thread will search this
					 * list for a free thread
					 */

void init_pool_sems ()
{
	mu_init (&sleep_till_free_thread);
	mu_lock (&sleep_till_free_thread);
}

/* add_free_thread ()
 *
 * return a thread to the pool.
 * If # free threads is more as maximum exit
 * else go to sleep
 *
 * p_thread	in	pointer to thread structure
 */
static void add_free_thread (p_thread)
struct pool *p_thread;
{
	int8 exit_now = FALSE;
	extern int nr_of_threads;

	mu_lock (&exclude_all_threads);
		if (nr_of_waiting_req == 0 && nr_freethreads == nr_of_threads)
			exit_now = TRUE;
		else {
			p_thread->thread_type = FREE;
			p_thread->jobnr = GENERATIC;

			if (nr_of_waiting_req > 0) {
				p_thread->nthread = threads_for_req;
				threads_for_req = p_thread;
				nr_of_waiting_req--;
				mu_unlock (&sleep_till_free_thread);
			} else {
				nr_freethreads++;
				p_thread->nthread = free_threads;
				free_threads = p_thread;
			}
		}
	mu_unlock (&exclude_all_threads);

	if (exit_now) {
		MON_EVENT ("free pool thread exit");
#ifdef DEBUG
		if (debug > 2) printf ("Free pool thread exit (free %d)\n",
								nr_freethreads);
#endif
		thread_exit ();
	}
	else {
#ifdef DEBUG
		if (debug > 2) printf ("Free pool thread going to sleep (free %d)\n",
								nr_freethreads);
#endif
		mu_lock (&p_thread->thread_sem);
	}
}

/* pool_thread ()
 * 
 * Main loop of all threads
 *
 * p_my_work	in	pointer to it's own thread structure
 */
/*ARGSUSED*/
static void pool_thread (p, s)
char * p;
int s;
{
	struct pool *p_my_work = (struct pool *) p;

	mu_init (&p_my_work->thread_sem);
	mu_lock (&p_my_work->thread_sem);
	for (;;) {
		switch (p_my_work->thread_type) {
		case WORK_TYPE:
			handle_work (p_my_work->jobnr);
			break;
		case REQ_TYPE:
			handle_req ();
			break;
		case AGE_TYPE:
			agejobs ();
			break;
#ifdef WITH_EXEC
		case EXEC_TYPE:
			dosak_exec ();
			break;
#endif
		case FREE:
			break;
		default:
			fprintf (stderr, "Bad thread type %d\n",
							p_my_work->thread_type);
			p_my_work->thread_type = FREE;
			break;
		}
		add_free_thread (p_my_work);
	}
}

/* alloc_free_thread (jobnr, type)
 *
 * allocate a thread from the pool for 'type' work
 * jobnr is only meaningfull if type == WORK
 *
 * jobnr	in	number of job
 * type		in	type of work
 * return		TRUE is succes, else FALSE
 */
int8 alloc_free_thread (jobnr, type)
objnum jobnr;
int type;
{
	struct pool *pwkr;

	mu_lock (&exclude_all_threads);
		if ((pwkr = free_threads) != (struct pool *) 0) {
			free_threads = free_threads->nthread;
			--nr_freethreads;
		}
	mu_unlock (&exclude_all_threads);

	if (pwkr == (struct pool *) 0)
		return FALSE;
	else {
#ifdef DEBUG
		if (type != WORK_TYPE) {
			if (debug > 1)
				printf ("Free pool thread allocated for %s (%d free threads left)\n", type == REQ_TYPE ? "handeling requests" : "garbage collection", nr_freethreads);
		} else {
			if (debug > 2)
				printf ("Free pool thread allocated for handeling work (%d free threads left)\n", nr_freethreads);
		}
#endif
		pwkr->jobnr = jobnr;
		pwkr->thread_type = type;
		mu_unlock (&pwkr->thread_sem);
	}
	return TRUE;
}

/* create_newthread (jobnr, type)
 *
 * create a new thread for 'type' work
 * jobnr is only meaningfull if type == WORK
 *
 * jobnr	in	number of job
 * type		in	type of work
 * return		TRUE is succes, else FALSE
 */
int8 create_newthread (jobnr, type)
objnum jobnr;
int type;
{
	struct pool *pwkr;

	if ((pwkr = (struct pool *) malloc (sizeof (struct pool))) ==
				(struct pool *) 0) {
		fprintf (stderr,
			 "Cannot allocate thread struct for new thread\n");
		return FALSE;
	}

	pwkr->thread_type = type;
	pwkr->jobnr = jobnr;

	if (! thread_newthread (pool_thread, POOL_STACK,
					(char *) pwkr, sizeof (struct pool))) {
		fprintf (stderr, "Can't start new server pool thread\n");
		return FALSE;
	}
	MON_EVENT ("create pool thread");
	return TRUE;
}

/* wait_for_free_thread (jobnr, type)
 *
 *	if free thread available use it
 *	else 
 *		create new thread 
 *		if failed
 *			wait until thread is available
 *
 * jobnr is only meaningfull if type == WORK
 *
 * jobnr	in	number of job
 * type		in	type of work
 */
void wait_for_free_thread (jobnr, type)
objnum jobnr;
int type;
{
	int8 created, do_wait;
	struct pool *pwkr;

	created = do_wait = FALSE;

	mu_lock (&exclude_all_threads);
		if ((pwkr = free_threads) != (struct pool *) 0) {
			free_threads = free_threads->nthread;
			--nr_freethreads;
		}
		else if (create_newthread (jobnr, type))
			created = TRUE;
		else {
			nr_of_waiting_req++;
			do_wait = TRUE;
		}
	mu_unlock (&exclude_all_threads);

	if (created) {
#ifdef DEBUG
		if (debug > 1) printf ("No free pool threads created new thread\n");
#endif
		MON_EVENT ("Out of pool threads");
		return;
	}

	if (do_wait) {
		MON_EVENT ("Waiting for free pool threads");
		fprintf (stderr, "Waiting for free pool threads");
		fprintf (stderr, "(could not create new threads)\n");
		mu_lock (&sleep_till_free_thread);

		mu_lock (&exclude_all_threads);
			if ((pwkr = threads_for_req) != (struct pool *) 0)
				threads_for_req = threads_for_req->nthread;
		mu_unlock (&exclude_all_threads);
	}

#ifdef DEBUG
	if (type != WORK_TYPE) {
		if (debug > 1)
			printf ("Free pool thread allocated for %s (%d free threads left)\n", type == REQ_TYPE ? "handeling requests" : "garbage collection", nr_freethreads);
	} else {
		if (debug > 2)
			printf ("Free pool thread allocated for handeling work (%d free threads left)\n", nr_freethreads);
	}
#endif
	pwkr->jobnr = jobnr;
	pwkr->thread_type = type;
	mu_unlock (&pwkr->thread_sem);
}
