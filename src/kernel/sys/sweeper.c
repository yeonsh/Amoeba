/*	@(#)sweeper.c	1.6	96/02/27 14:23:27 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <sys/proto.h>

typedef struct {
	void (*t_sweep)();
	long t_arg;
	interval t_interval;
	interval t_clock;
	int t_once;
} sweep_t, *sweep_p;

#define MIN(a,b)	((a < b) ? a : b)

#ifndef MAXSWEEP
#define MAXSWEEP	20		/* Size of sweeper table */
#endif

#define DEFMINSWEEP	10000

static sweep_t sweepertab[MAXSWEEP];
static int nsweeper;

/*
 * Use of counters:
 *
 * min is supposed to be the minimum interval where actual action
 *	is required.
 * tot_ms_passed is used for counting upwards while nothing happens
 * milli_uptime is external for the rest of the kernel
 *
 * All times in milliseconds
 */

unsigned long milli_uptime;

static int min;
static int tot_ms_passed;
static unsigned long (*hw_getmilli)();

unsigned long getmilli() {

	if (hw_getmilli)
		return (*hw_getmilli)();
	return milli_uptime;
}

void
set_hw_milli(f)
unsigned long (*f)();
{

	hw_getmilli = f;
}

void
sweeper_set(sweep, arg, period, once)
    void (*sweep)();
    long arg;
    interval period;
    int once;
{
    sweep_p t;
    
    for(t = sweepertab; t < sweepertab + MAXSWEEP; t++)
	if(!t->t_sweep) {
	    nsweeper++;
	    t->t_sweep = sweep;
	    t->t_arg = arg;
	    t->t_interval = period;
	    t->t_clock = period + tot_ms_passed;
	    t->t_once = once;
	    min = 0;	/* force action on next sweeper_run */
	    return;
	}
    panic("sweeper_set: out of entries");
}

/*
 * We are informed that ms_passed milliseconds have passed since
 * the previous call.
 */

void
sweeper_run(ms_passed)
long ms_passed;
{
    register sweep_p t;
    int newmin;
    
    milli_uptime += ms_passed;
    tot_ms_passed += ms_passed;
    if(tot_ms_passed < min)
	return;
    /*
     * Check sweepertab. Something is bound to happen.
     */
    newmin = DEFMINSWEEP;
    for(t = sweepertab; t < sweepertab + MAXSWEEP; t++) {
	if(!t->t_sweep) continue;
	t->t_clock -= tot_ms_passed;
	if(t->t_clock <= 0) {
	    /*
	     * Execute routine, and reset routine clock by adding interval
	     * Only problem is if clock then still is negative:
	     * should we continue to execute it?
	     * For now we print a warning message.
	     */
	    (*t->t_sweep)(t->t_arg);

	    if(t->t_once) {
		t->t_sweep = 0;	/* One shot, clear entry */
		nsweeper--;
		continue;
	    } else {
		t->t_clock += t->t_interval;
		if (t->t_clock <= 0) {
			t->t_clock = t->t_interval + tot_ms_passed;
#ifndef NDEBUG
			printf("Sweeper problem with %lx(%lx)\n",
				t->t_sweep, t->t_arg);
#endif
		}
	    }
	}
	newmin = MIN(newmin, t->t_clock);
    }
    min = newmin;
    tot_ms_passed = 0;
}

#ifndef SMALL_KERNEL

sweeper_dump(begin, end)
char *begin;
char *end;
{
    sweep_p t;
    register char *p;
    
    p = bprintf(begin, end, "=========== sweeper dump ==========\n");
    p = bprintf(p, end, "nsweeper: %d min-interval %d\n", nsweeper, min);
    for(t = sweepertab; t < sweepertab + MAXSWEEP; t++)
	if(t->t_sweep) {
	    p = bprintf(p, end, "proc: %lx(%d), %d, %d, %d\n", t->t_sweep,
		   t->t_arg, t->t_interval, t->t_clock,
		   t->t_once);
	}
    return p - begin;
}

#endif
