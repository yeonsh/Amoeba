/*	@(#)sun4m_timer.c	1.1	96/02/27 11:30:05 */
/*
 * Interface routines for the user timer on the sun4m machines.
 */

#include <amoeba.h>
#include <stdio.h>
#include <stdlib.h>
#include <module/host.h>
#include <module/direct.h>
#include <module/proc.h>
#include <proc.h>
#include <stderr.h>
#include <machdep.h>
#include <machdep/dev/sun4m_timer.h>

#define TIMER_SEG_SIZE	0x10000	/* 64K click size */

static struct sun4m_timer_regs *timer_regptr;

/* no public prototype for this one: */
extern char *findhole();

errstat
sun4m_timer_init(hostname)
char *hostname;
{
    capability hostcap;
    capability timercap;
    char *segptr;
    segid seg;
    errstat err;
    static int inited;

    if (!inited) {
	inited = 1;
    } else {
	return STD_NOTNOW;
    }

    if (hostname == NULL) {
	/* try hostname supplied by ax or gax: */
	hostname = getenv("AX_HOST");
    }
    if (hostname == NULL) {
	fprintf(stderr, "sun4m_timer: no hostname specified\n");
	return STD_ARGBAD;
    }

    err = host_lookup(hostname, &hostcap);
    if (err != STD_OK) {
	fprintf(stderr, "sun4m_timer: cannot lookup host `%s' (%s)\n",
		hostname, err_why(err));
	return err;
    }

    err = dir_lookup(&hostcap, "usertimer", &timercap);
    if (err != STD_OK) {
	fprintf(stderr, "sun4m_timer: cannot find usertimer on %s (%s)\n",
	        hostname, err_why(err));
	return err;
    }

    segptr = findhole((long) TIMER_SEG_SIZE);
    if (segptr == NULL) {
	fprintf(stderr, "sun4m_timer: cannot find memory hole\n");
	return STD_NOMEM;
    }

    /* We map the timer segment read-write in so that we can also reset,
     * stop and restart it.
     */
    seg = seg_map(&timercap, (vir_bytes) segptr, (vir_bytes) TIMER_SEG_SIZE,
		  (long) (MAP_TYPEDATA | MAP_READWRITE | MAP_INPLACE));
    if (seg < 0) {
	fprintf(stderr, "sun4m_timer: cannot map timer on %s (%s)\n",
		hostname, err_why((errstat) seg));
	return (errstat) seg;
    }

    timer_regptr = (struct sun4m_timer_regs *) segptr;
    /* make sure it's running */
    timer_regptr->pc_ustartstop = 1;

    return STD_OK;
}

struct sun4m_timer_regs *
sun4m_timer_getptr()
{
    /* return a pointer to the mapped-in timer registers */
    return timer_regptr;
}

long
sun4m_timer_diff(start, stop)
union sun4m_timer *start, *stop;
{
    /* Convert difference in timer values to a number of microseconds.
     * The timer adds 0x200 (1 << 9) to the lsw every 500 nsec
     * (i.e. twice every microsec) so one bit difference in the msw amounts
     * to (1 << (32 - 9)) * 500 nsec = (1 << (32 - 9)) / 2 usec.
     */
    long nmicrosec;
    long msw_diff;
    uint32 lsw_diff;

    msw_diff = stop->tm_msw - start->tm_msw;
    if (msw_diff < 0) {
	/* timer running backwards?! */
	return -1;
    }
    if (stop->tm_lsw < start->tm_lsw) {
	/* lsw diff negative */
	msw_diff--;
    }

    nmicrosec = msw_diff * ((1 << (32 - 9)) / 2);
    lsw_diff = stop->tm_lsw - start->tm_lsw;
    nmicrosec += ((lsw_diff >> 9) / 2);
    return nmicrosec;
}

uint32
sun4m_timer_micro()
{
    /* Convenience function that directly delivers a microsecond
     * granularity time value.  If speed really is an issue, the
     * caller should do something similar inline.
     */
    union sun4m_timer timer;	/* to copy timer value into */
    
    copy_timer(&timer_regptr->pc_timer, &timer.tm_val);
    return (timer.tm_lsw >> 10) | (timer.tm_msw << (32 - 10));
}

