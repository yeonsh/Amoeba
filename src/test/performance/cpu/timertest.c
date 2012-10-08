/*	@(#)timertest.c	1.1	96/02/27 10:56:28 */
/*
 * Test program for the new user timer on the sun4m machines.
 */

#include <amoeba.h>
#include <stdio.h>
#include <stderr.h>
#include <stdlib.h>
#include <module/syscall.h>
#include <unistd.h>
#include <math.h>
#include <machdep/dev/sun4m_timer.h>

void
time_report(start, stop, microstart, microstop, millistart, millistop)
union sun4m_timer *start, *stop;
uint32 microstart, microstop;
uint32 millistart, millistop; /* as a sanity check */
{
    printf("start = [0x%lx, 0x%lx], ", start->tm_msw, start->tm_lsw);
    printf("stop = [0x%lx, 0x%lx] ", stop->tm_msw, stop->tm_lsw);
    printf("-> %ld microsecs\n", sun4m_timer_diff(start, stop));
    printf("sun4m_timer_micro(): 0x%lx - 0x%lx = %ld microsecs\n",
	   microstop, microstart, microstop - microstart);
    printf("sys_milli(): %ld millisecs\n", millistop - millistart);
}

void
timerloop()
{
    /*
     * We do a number of simple timing tests, using
     * 1 - bare 64 bit timer values
     * 2 - converted micro second values
     * 3 - as a sanity check: milli seconds from sys_milli()
     */
    struct sun4m_timer_regs *timer_regptr;
    pc_timer_type *counter_ptr;
    union sun4m_timer timer0, timer1;	/* to copy timer value into */
    pc_timer_type *timer0_ptr, *timer1_ptr;
    uint32 micro0, micro1;
    uint32 milli0, milli1;
    int i;

    /* set up pointers to timer and destination value for fast access */
    timer_regptr = sun4m_timer_getptr();
    counter_ptr = &timer_regptr->pc_timer;
    timer0_ptr = &timer0.tm_val;
    timer1_ptr = &timer1.tm_val;

#define start_timers(mil, mic, tim) {	\
    mil = sys_milli();			\
    mic = sun4m_timer_micro();		\
    copy_timer(counter_ptr, tim);	\
}
#define stop_timers(mil, mic, tim) {	\
    copy_timer(counter_ptr, tim);	\
    mic = sun4m_timer_micro();		\
    mil = sys_milli();			\
}

    /* simple sin() timing test */
    printf("computing 10000 sin()s\n");
    start_timers(milli0, micro0, timer0_ptr);
    for (i = 0; i < 10000; i++) {
	(void) sin(3.0);
    }
    stop_timers(milli1, micro1, timer1_ptr);
    time_report(&timer0, &timer1, micro0, micro1, milli0, milli1);

    /* Time a small number of sin() calls. Note that this would be imposible
     * to do with a sys_milli() timer.
     */
    for (i = 0; i < 5; i++) {
	int j;

	printf("computing 5 sin()s\n");
	start_timers(milli0, micro0, timer0_ptr);
	for (j = 0; j < 5; j++) {
	    (void) sin(3.0);
	}
	stop_timers(milli1, micro1, timer1_ptr);
	time_report(&timer0, &timer1, micro0, micro1, milli0, milli1);
    }

    /* and a few sleep() tests */
    printf("sleeping 5 secs..\n");
    start_timers(milli0, micro0, timer0_ptr);
    sleep(5);
    stop_timers(milli1, micro1, timer1_ptr);
    time_report(&timer0, &timer1, micro0, micro1, milli0, milli1);

    for (i = 0; i < 5; i++) {
	printf("sleeping 1 sec..\n");
	start_timers(milli0, micro0, timer0_ptr);
	sleep(1);
	stop_timers(milli1, micro1, timer1_ptr);
	time_report(&timer0, &timer1, micro0, micro1, milli0, milli1);
    }
}

main()
{
    errstat err;

    /* Initialize user timer module.
     * We assume that the hostname can be found from the environment.
     * Both ax and gax provide an "AX_HOST" environment variable.
     */
    err = sun4m_timer_init((char *) NULL);
    if (err != STD_OK) {
	fprintf(stderr, "sun4m_timer initialisation failed (%s)\n",
		err_why(err));
	exit(1);
    }

    /* start with a simple test using the free running timer */
    timerloop();
    timerloop();

#ifdef TIMER_CONTROL
    /* This tests the other features of the timer:
     * stop, restart, and reinitialization.
     * Since they may hamper other processes running on the same
     * host, they shouldn't be used normally.
     */
    
    {
	/* get a pointer to the mapped-in timer registers */
	struct sun4m_timer_regs *timer_regptr;

	timer_regptr = sun4m_timer_getptr();

	/* see if stop and restart works: */
	timer_regptr->pc_ustartstop = 0;
	printf("stopped clock\n");
	timerloop();

	printf("restarting clock\n");
	timer_regptr->pc_ustartstop = 1;
	timerloop();

	printf("resetting clock\n");
	timer_regptr->pc_timer_msw = 0;
	timer_regptr->pc_timer_lsw = 0;
	timerloop();
    }
#endif

    exit(0);
}
