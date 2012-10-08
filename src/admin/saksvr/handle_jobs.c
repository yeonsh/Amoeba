/*	@(#)handle_jobs.c	1.2	94/04/06 11:52:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "saksvr.h"

static void wakeup_server _ARGS((void));
static void sleep_server _ARGS((time_t));


static struct sak_job *runlist;	

/* remove job  from runlist
 * jobnr	in	job nr of job to be removed
 * assumes job_list_sem locked !
 */
void remove_from_runlist (jobnr)
objnum jobnr;
{
	struct sak_job *ptr, *prev;

	prev = NILCELL;
	ptr = runlist;
	while (ptr != NILCELL && ptr->ident != jobnr) {
		prev = ptr;
		ptr = ptr->time_next;
	}

	if (ptr != NILCELL && ptr->ident == jobnr) {
#ifdef DEBUG
		if (debug > 2)
			printf ("Descheduled : job %ld named %s\n",
				(long) jobnr, ptr->opt.name);
#endif
		if  (ptr == runlist)
			runlist = runlist->time_next;
		else
			prev->time_next = ptr->time_next;
	}
#ifdef DEBUG
	else if (debug > 2)
		printf ("Deschedule : job %ld not found\n", (long) jobnr);
#endif

	return;
}

/* insert job into runlist at the appropriate place (ordered by next
 * execution date)
 * assumes a locked joblist !! (job_list_sem)
 * pjob		in	pointer to job to be inserted
 */
void insert_runlist (pjob)
struct sak_job *pjob;
{
	struct sak_job *ptr, *prev;

	ptr = runlist;
	prev = NILCELL;
	while (ptr != NILCELL && ptr->executiondate <= pjob->executiondate) {
		prev = ptr;
		ptr = ptr->time_next;
	}

	if (ptr == runlist) {
		pjob->time_next = runlist;
		runlist = pjob;
		wakeup_server ();
	}
	else {
		pjob->time_next = ptr;
		prev->time_next = pjob;
	}
}

/* remove first job from runlist if it has to be executed now
 * return jobnr of this job
 * now		in	current time
 * return		jobnr of job to be run or NONE
 */
static objnum next_to_run (now)
time_t now;
{
	objnum jobnr;

	mu_lock (&job_list_sem);
		if (runlist != NILCELL && runlist->executiondate <= now) {
			jobnr = runlist->ident;
			runlist = runlist->time_next;
		} else
			jobnr = NONE;

	mu_unlock (&job_list_sem);

	return (jobnr);
}

/* actual server. sleeps till next event (or MAX_SLEEP) and then locates
 * a thread to execute the job
 */
void handle_jobs ()
{
	time_t clock, sleeptime;
	objnum jobnr;

	for (;;) {
		time (&clock);
		sleeptime = MAX_SLEEP;
		mu_lock (&job_list_sem);
			if (runlist != NILCELL) {
				sleeptime = runlist->executiondate - clock;
				if (sleeptime > MAX_SLEEP)
					sleeptime = MAX_SLEEP;
			}
		mu_unlock (&job_list_sem);

		/* execute everything within 5 seconds */
		if (sleeptime > 5) {
			if (sleeptime > MAX_SLEEP)
				sleeptime = MAX_SLEEP;
#ifdef DEBUG
			if (debug > 1) {
				printf ("Main thread : nothing to do sleeping %ld seconds%s\n", sleeptime, (runlist == NILCELL) ? " (Empty runlist)" : "");
			}
#endif

			sleep_server (sleeptime * SEC);
			continue;
		}

#ifdef DEBUG
		if (debug > 2) {
			extern mutex tm_sem;

			mu_lock (&tm_sem);
			printf ("Main thread : Checking for work %s",
						ctime (&clock));
			mu_unlock (&tm_sem);
		}
#endif

		clock += 5;
		while ((jobnr = next_to_run (clock)) != NONE) {
#ifdef DEBUG
			if (debug > 2) printf ("Main thread : work on jobnr %ld\n", jobnr);
#endif
			wait_for_free_thread (jobnr, WORK_TYPE);
		}
	}
}

static int8 woke_up_server = FALSE;
static mutex server_sem, server_stat;

void init_server_sleep ()
{
	mu_init (&server_sem);
	mu_lock (&server_sem); /* make sure it's locked */
}

/* wakeup server used by request thread if a job is inserted in front of
 * the runlist
 */
static void wakeup_server ()
{
	mu_lock (&server_stat);
	if (!woke_up_server) {
#ifdef DEBUG
		if (debug > 1) printf ("Server is sleeping waking it up (got new job)\n");
#endif
		mu_unlock (&server_sem);
		woke_up_server = TRUE;
	}
#ifdef DEBUG
	else if (debug > 2) printf ("Server WAS sleeping .... \n");
#endif
	mu_unlock (&server_stat);
}

/* used by the server (handle_jobs) to sleep the specified amount of seconds
 * sleep can be interrupted by request thread (wakeup_server) if runlist changes
 * sec		in	# seconds to sleep
 */
static void sleep_server (sec)
time_t sec;
{
	(void) mu_trylock (&server_sem, sec);

	mu_lock (&server_stat);
	if (woke_up_server) {
		/* make sure server_sem is in a locked state. If server is
		 * unlocked by a thread, just after the 
		 * mu_trylock (&server_sem, sec) timed out the server_sem is
		 * left in a unlocked stated. If we the reset woke_up_server
		 * this could result in an unlock of a already unlocked mutex
		 */
		(void) mu_trylock (&server_sem, (interval) 0);
		woke_up_server = FALSE;
	}
	mu_unlock (&server_stat);
}

#ifdef DEBUG
void debug_list_runlist ()
{
	extern mutex tm_sem;
	struct sak_job *pjob;

	mu_lock (&job_list_sem);
	mu_lock (&tm_sem);

	if (runlist == NILCELL)
		printf ("Runlist empty\n");
	else
		for (pjob = runlist; pjob != NILCELL; pjob = pjob->time_next)
			printf (
			     "job %ld named %s (%scatchup,%s result) when : %s",
			     (long) pjob->ident, pjob->opt.name,
			     pjob->opt.catchup ? "" : "no ",
			     pjob->opt.save_result ? "save" : "discard",
			     ctime (&pjob->executiondate));

	mu_unlock (&tm_sem);
	mu_unlock (&job_list_sem);
}
#endif
