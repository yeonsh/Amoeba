/*	@(#)timer.c	1.7	96/02/27 10:11:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "monitor.h"
#include "cmdreg.h"
#include "stderr.h"
#include "semaphore.h"
#include "posix/limits.h"
#include "svr.h"



    /************************************************************\
     *								*
     *	Timer normalisation to defend against timer wraps	*
     *								*
    \************************************************************/

semaphore n_timers;

/* We wrap the timer around after 1 << TM_SHIFT ticks.
 * When this happens we modify the or_nextpoll and or_nextboot fields
 * so that the "later" function keeps providing consistent results.
 */
#define TM_SHIFT	28
#define TM_MASK		((1 << TM_SHIFT) - 1)

int
later(t0, t1)
Time_t t0;
Time_t t1;
{
    /* Return 1 if the time indicated by t0 is later than t1.  Else 0. */
    return (t0 > t1);
}

Time_t
add_time(t0, incr)
Time_t t0;
Time_t incr;
{
    Time_t new;

    new = t0 + incr;
    if (new > t0) {
	return new;
    } else {
	/* don't add it when it overflows */
	return t0;
    }
}

static void
do_wrap(t)
Time_t *t;
{
    /* Only wrap the time when it is non-negative (otherwise the timestamp
     * already refers to the past).  This also avoids the slight possibility
     * of it suddenly becoming positive again.
     */
    if (*t >= 0) {
	Time_t new = *t - (Time_t) TM_MASK;

	if (debugging) {
	    prf("%ndo_wrap: replace %d by %d\n", (int) *t, (int) new);
	}
	*t = new;
    }
}

static void
maybewrap()
{
    static int inited = 0;
    static unsigned long saved_bit;
    unsigned long interval_bit;

    interval_bit = (sys_milli() & (1 << TM_SHIFT));
    if (!inited) {
	saved_bit = interval_bit;
	inited = 1;
    } else if (interval_bit != saved_bit) {
	/* the TM_SHIFT bit flipped */
	int i;

	saved_bit = interval_bit;
	MON_EVENT("Timer wrap");

	for (i = 0; i < MAX_OBJ; ++i) {
	    mu_lock(&objs[i].or_lock);

	    if (obj_in_use(&objs[i])) {
		do_wrap(&objs[i].or.or_nextpoll);
		do_wrap(&objs[i].or.or_nextboot);
	    }

	    MU_CHECK(objs[i].or_lock);
	    mu_unlock(&objs[i].or_lock);
	}
    }
} /* wraptime */

Time_t
my_gettime()
{
    return (Time_t) (sys_milli() & TM_MASK);
} /* my_gettime */

void
TimerInit()
{
    int dummy, success;
    extern semaphore start_service;
    interval prev_to;
    errstat err;
    int len;
    char info[200];

    sema_init(&n_timers, 0);
    /* This computes the service port as well */
    if ((err = impl_boot_reinit((header *) NULL, (obj_ptr) NULL,
			    &dummy, &dummy, &success)) != STD_OK) {
	prf("%nreinit doesn't work: %s\n", err_why(err));
	exit(1);
    }
    if (!success) {
	extern int LineNo;
	prf("%nCannot init; last line read: %d\n", LineNo);
	exit(1);
    }
    prev_to = timeout((interval) 2000);	/* A short timeout */
    do {
	errstat shuterr;
	while ((err = std_info(&putcap, info, sizeof(info) - 1, &len))
							== RPC_FAILURE)
	    MON_EVENT("Init-touch: RPC_FAILURE");

	if (err != RPC_NOTFOUND) {
	    if (err == STD_OK) info[len] = '\0';
	    shuterr = boot_shutdown(&putcap);
	    if (shuterr == RPC_FAILURE)
		prf("%nPrevious incarnation: RPC-failure\n");
	    else if (shuterr != STD_OK) {
		if (err == STD_OK)
		    prf("%nPrevious incarnation's info: \"%s\"\n", info);
		prf("%nPrevious incarnation not willing to leave (%s)\n",
							err_why(shuterr));
		exit(2);
	    } else if (verbose) prf("%nprevious bootserver shutdown\n");
	}
    } while (err != RPC_NOTFOUND);
    timeout(prev_to);
    ReadProcCaps();
    /* Kill and create the name mapping again - the objnums have changed */
    (void) KillMap();
    MkMap(objs, SIZEOFTABLE(objs));
    /* Start service threads */
    sema_mup(&start_service, N_SVR_THREADS);
    prf("BOOTSERVER INITIALIZED\n");
} /* TimerInit */


/*
 *	This thread looks for processes whose known state is quite old
 */
void
TimerThread(param, parsize)
char *param;
int   parsize;
/*ARGSUSED*/
{
    extern void list_badport();
    extern void reset_badport();
    extern void SaveProcCaps();
    mutex sleeper;
    extern int SaveState;
    static thread_counter = 0;
    static mutex thread_mu;
    int thread_id, nloops = 0;

    intr_init();	/* Because prf() doesn't know who calls it */
    mu_init(&sleeper); mu_lock(&sleeper);
    mu_lock(&thread_mu);
    /* Thread 0 checks all, the others are just there to speed things up */
    thread_id = thread_counter++;
    MU_CHECK(thread_mu);
    mu_unlock(&thread_mu);
    timeout((interval) 10000);

    while (!closing) {
	int i, handled;
	static Time_t now;	/* Shared! */
	Time_t soon;

	MON_EVENT("top of timer loop");
	maybewrap();
	now = my_gettime();
	if (thread_id == 0) {
	    if (debugging) list_badport();
	    reset_badport();
	}
	soon = add_time(now, (Time_t)10000); /*Upperbound to sleep at the end*/
	handled = 0;
	for (i = 0; !closing && i < MAX_OBJ; ++i) {
	    obj_rep *bp;

	    bp = &objs[i];
	    if (obj_in_use(bp) && !(bp->or.or_data.flags & BOOT_INACTIVE) &&
		(mu_trylock(&bp->or_lock, (interval)(thread_id ? 0 : -1))== 0))
	    {
		if (!later(bp->or.or_nextpoll, now)) {
		    MON_EVENT("timer poll");
		    ++handled;
		    bp->or.or_errstr[0] = '\0';
		    switch (Poll(bp)) {
		    case POLL_RUNS:
			bp->or.or_status |= ST_POLLOK;
			break;
		    case POLL_DOWN:
			bp->or.or_status &= ~ST_POLLOK;
			if (verbose)
			    prf("%n%s considered down\n", bp->or.or_data.name);
			do_boot(bp);
			break;
		    case POLL_MAYBE:
			bp->or.or_status &= ~ST_POLLOK;
			if (++bp->or.or_retries >= MAX_RETRIES) {
			    MON_EVENT("Process failed too often; reboot");
			    if (verbose) {
				prf("%nProcCap %s failed too often\n",
				    bp->or.or_data.name);
			    }
			    do_boot(bp);
			}
			break;
		    case POLL_CANNOT:
			bp->or.or_status &= ~ST_POLLOK;
			break;
		    default:
			prf("%nTimer: bad case in switch\n");
			abort();
		    }
		    if (later(now, bp->or.or_nextpoll)) {
			MON_EVENT("timer(): adjusting nextpoll");
			bp->or.or_nextpoll =
			       add_time(my_gettime(), bp->or.or_data.pollrate);
		    }
		} else {
		    /* Should not be polled yet */
		    if (later(soon, bp->or.or_nextpoll)) {
			MON_EVENT("Adjust because poll");
			soon = bp->or.or_nextpoll;
		    }
#if 0
		    if (later(soon, bp->or.or_nextboot)) {
			MON_EVENT("Adjust because boot");
			soon = bp->or.or_nextboot;
		    }
#endif
		}
		MU_CHECK(bp->or_lock);
		mu_unlock(&bp->or_lock);
	    }
	}
	if (thread_id == 0) {
	    if (SaveState) SaveProcCaps();
	    /*
	     * Turn off verbose flag after 5 passes:
	     * The value '2' for verbose is to distinguish
	     * between values set with boot_ctl
	     */
	    if (nloops == 5 && verbose == 2) {
		++nloops;
		prf("%nauto-switch off verbose\n");
		verbose = 0;
	    } else if (nloops < 5) ++nloops;
	}

	if (!closing) {
	    Time_t verysoon;

	    /* Wait at least half a second to avoid an avalanche of
	     * polls in case of 'silly' pollrates.
	     */
	    verysoon = add_time(now, (Time_t) 500);
	    if (later(verysoon, soon)) {
		MON_EVENT("minimal sleep time");
		soon = verysoon;
	    }

	    mu_trylock(&sleeper, (interval) (soon - now));
	} else {
	    switch (handled) {
	    case 0:
		if (thread_id != 0) {
		    MON_EVENT("extra timerthread: I feel so useless...");
		    mu_trylock(&sleeper, (interval) 5000);
		} else MON_EVENT("main timerthread inspected 0");
		break;
	    case 1:
		MON_EVENT("timerthread inspected 1");
		break;
	    case 2:
		MON_EVENT("timerthread inspected 2");
		break;
	    case 3:
		MON_EVENT("timerthread inspected 3");
		break;
	    default:
		if (thread_id) MON_EVENT("extra timerthread inspected > 3");
		else MON_EVENT("main timerthread inspected > 3");
		break;
	    }
	}
    }
    if (debugging) prf("%nTimerthread exit\n");
    sema_up(&n_timers);
    thread_exit();
} /* TimerThread */
