/*	@(#)alarm.c	1.4	96/02/27 13:13:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* last modified apr 20 93 Ron Visser */
#include <amtools.h>
#include <semaphore.h>
#include <ajax/mymonitor.h>
#include <sys/types.h>
#include <ampolicy.h>
#include <pwd.h>
#include <capset.h>
#include <module/buffers.h>
#include <sp_dir.h>
#include <signal.h>

#include "session.h"
#include "alarm.h"

#define NIL_TIMER	((struct timer *) 0)

struct timer
{
	unsigned long tm_due;
	pid_t tm_pid;
	struct timer *tm_next;
};

struct timer *tm_list = NIL_TIMER;

static mutex mu_timer;
static semaphore al_sleep;

static void
timer_insert(tp)
struct timer *tp;
{
/* inserts a struct timer in the sorted linked list of timers */
  struct timer **pred; /* pred contains a pointer to the address where
			* the pointer to the struct will be stored, this 
			* can be the next field of its predecessor 
			* or the pointer to the list when it is empty 
			*/

  mu_lock(&mu_timer);
  for(pred = &tm_list; *pred != NIL_TIMER; pred = &(*pred)->tm_next)
	if((*pred)->tm_due > tp->tm_due) break;

  tp->tm_next = (*pred == NIL_TIMER) ? NIL_TIMER : (*pred)->tm_next;
  *pred = tp;
  mu_unlock(&mu_timer);

  /* the list  has a new head, wake up the other thread */
  if(pred == &tm_list)
	sema_up(&al_sleep);

}

static struct timer *
timer_unlink(pid)
pid_t pid;
{
/* finds a timer with the same capability (same port and same private number)
 * unlinks it and returns a pointer to it for updating or deletion
 * if no timer is found a nil pointer is returned
 * the port determines the session server to use and the private number
 * contains the pid of the process belonging to that session server
 */
  struct timer **pred; /* pred contains a pointer to the address where
			* the pointer to the struct will be stored, this 
			* can be the next field of its predecessor 
			* or the pointer to the list when it is empty 
			*/
  struct timer *found;

  mu_lock(&mu_timer);
  for(pred = &tm_list; *pred != NIL_TIMER; pred = &(*pred)->tm_next)
	if(pid == (*pred)->tm_pid)
	{
		found = *pred;
		*pred = (*pred)->tm_next;
		mu_unlock(&mu_timer);
		return found;
	}
  mu_unlock(&mu_timer);
  return NIL_TIMER;
}


/* check if the return value is ok in all cases */
int
set_timer(pid, nsec)
pid_t pid;
unsigned int nsec;
{
  unsigned long prev,curr;
  struct timer *tp;

  curr = sys_milli();
  tp = timer_unlink(pid);
  if(nsec != 0)
  {
	if(tp == NIL_TIMER)
	{
		if((tp = (struct timer *)malloc(sizeof(struct timer))) == NULL)
		{
			MON_EVENT("set_timer: cannot malloc");
			return 0;
		}
		prev = curr;
	} else
		prev = tp->tm_due;
	tp->tm_pid = pid;
	tp->tm_due = curr + nsec * TICKSPERSEC;
	timer_insert(tp);
  } else {
	if(tp != NIL_TIMER)
	{
		prev = tp->tm_due;
		free((_VOIDSTAR) tp);
	} else
		prev = curr;
  }
	
  return (int)((prev - curr) / TICKSPERSEC);
}


/*ARGSUSED*/
static void
alarmsvr(p, s)
char *	p;
int	s;
{
  errstat err;
  int victpid;
  unsigned long delta;

  sema_init(&al_sleep,0);
  for(;;){
		mu_lock(&mu_timer);
	if(tm_list == NIL_TIMER)
	{
		/* sleep until there is an insertion */
			mu_unlock(&mu_timer);
		sema_down(&al_sleep);
	} else {
		delta = tm_list->tm_due - sys_milli();
		if(sys_milli() < tm_list->tm_due /*delta > 0*/)
		/* to catch expired timers (delta is unsigned) */
		{
			/* wait until the next alarm expires but if it expires
			 * check if the world has changed while we slept
			 */
			mu_unlock(&mu_timer);
			sema_trydown(&al_sleep, (interval) delta);
		} else {
			extern errstat ses_signal_pid();

			/* do the real work outside the critical section */
			victpid = tm_list->tm_pid;
			tm_list = tm_list->tm_next;
			mu_unlock(&mu_timer);

			/* call signalling routine in sesimpl.c */
			if ((err = ses_signal_pid(victpid, SIGALRM)) != STD_OK)
			{
				if (err == STD_NOTFOUND) {
					MON_EVENT("alarm: no such process");
				}
				else {
					MON_EVENT("alarm: unknown failure");
				}
			}
		}
	}
  }
}


void start_alrmsvr()
{
        if (!thread_newthread(alarmsvr, 4000, (char *)0, 0))
                warning("cannot start alarm server");
}

