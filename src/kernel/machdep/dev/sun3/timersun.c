/*	@(#)timersun.c	1.7	96/02/27 13:53:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "map.h"
#include "memaccess.h"
#include "randseed.h"
#include "sys/proto.h"
#include "arch_proto.h"
#include "assert.h"
INIT_ASSERT

#define CLOCK	ABSPTR(volatile struct intersil *, IOBASE + 0x6000)
#define INTREG	ABSPTR(volatile char *, IOBASE + 0xA000)

#define bit(b)	(1 << (b))

#define CBIT	bit(5)

#define BLEEPTIME	2

/* command bits */
#define CLOCKTEST	bit(5)
#define INTENABLE	bit(4)
#define CLOCKRUN	bit(3)
#define CLOCK24		bit(2)
#define FREQ		0x3

/* status/mask bits */
#define PERDAY		bit(6)
#define PERHOUR		bit(5)
#define PERMINUTE	bit(4)
#define PERSECOND	bit(3)
#define PERTENTH	bit(2)
#define PERHUNDREDTH	bit(1)
#define SETALARM	bit(0)

/* When you change TICKS_PER_SEC be sure to adjust the HZ_MASK too! */
#define	TICKS_PER_SEC	100
#define	HZ_MASK		PERHUNDREDTH


struct timeval {
	char tv_hundredths;
	char tv_hours;
	char tv_minutes;
	char tv_seconds;
	char tv_month;
	char tv_date;
	char tv_year;
	char tv_day_of_week;
};

struct intersil {
	struct timeval is_counter;
	struct timeval is_ram;
	char is_statmask;
	char is_command;
	char is_unused[14];
};

static bleeptime;

bleep(){
  disable();
  if (bleeptime == 0)
	startbleep();
  bleeptime += BLEEPTIME;
  enable();
}

#ifdef MEASURE
static long realtime_dsec;

sun3_hwtimer()
{
    long prev, cur;

    do { 
	prev = realtime_dsec;
	cur = realtime_dsec;
    } while (cur != prev);
    return(cur);
}
#endif

#ifdef PROFILE

/* The sun3 doesn't seem to have a second timer which we can use for
 * profiling purposes.  Currently we just use the timer at the regular
 * rate to fake profile interrupts.
 * While profiling we could temporarily increase the timing rate and
 * generate a sweeper run every 5 ticks or so, but this has not been
 * implemented yet.
 */
static int currently_profiling;

void
start_prof_tim(milli)
interval milli;
{
    currently_profiling = 1;
}

void
stop_prof_tim()
{
    currently_profiling = 0;
}

#endif /* PROFILE */

#ifndef PROFILE
/*ARGSUSED*/
#endif
static void
inttimer(sp)
struct stframe sp;
{
  char x = mem_get_byte(&CLOCK->is_statmask), intreg = mem_get_byte(INTREG);

  mem_put_byte(INTREG, intreg & ~CBIT);	/* clear interrupt bit */
  mem_put_byte(INTREG, intreg);
#ifdef MEASURE
  realtime_dsec++;
#endif
  x = mem_get_byte(&CLOCK->is_statmask);       /* in case we got another one */
  enqueue(sweeper_run, 1000L / TICKS_PER_SEC);
  if (bleeptime != 0 && --bleeptime == 0)
	stopbleep();
#ifdef PROFILE
  if (currently_profiling)
	profile_timer(sp);
#endif
}

void
inittimer(){
  char x;

  (void) setvec(29, (void (*) _ARGS((int))) inttimer);
  assert(TICKS_PER_SEC <= 1000);
  mem_put_byte(&CLOCK->is_statmask, HZ_MASK);
  mem_put_byte(&CLOCK->is_command, CLOCKRUN | CLOCK24);
  x = mem_get_byte(&CLOCK->is_statmask);	/* clear interrupt status */
  mem_put_byte(&CLOCK->is_command, INTENABLE | CLOCKRUN | CLOCK24);
  mem_OR_byte(INTREG, CBIT);
  randseed((unsigned long) CLOCK->is_counter.tv_hundredths, 7, RANDSEED_INVOC);
  randseed((unsigned long) CLOCK->is_counter.tv_seconds, 6, RANDSEED_INVOC);
  randseed((unsigned long) CLOCK->is_counter.tv_minutes, 6, RANDSEED_INVOC);
  randseed((unsigned long) CLOCK->is_counter.tv_hours, 5, RANDSEED_INVOC);
}

#ifndef SUN_HW_CLOCK
/*
 * rest of the routines here use the Intersil as storage for a
 * calendar clock.
 */

#include "time.h"
#include "timerdev.h"

#define CHIP0YEAR	68	/* That's what SunOs does anyway */

read_hardware_clock(secp, msecp)
long *secp;
int *msecp;
{
	struct tm tm;
	long unixtime();

	*msecp = 10 * CLOCK->is_counter.tv_hundredths;
	tm.tm_sec = CLOCK->is_counter.tv_seconds;
	tm.tm_min = CLOCK->is_counter.tv_minutes;
	tm.tm_hour = CLOCK->is_counter.tv_hours;
	tm.tm_mday = CLOCK->is_counter.tv_date;
	tm.tm_mon = CLOCK->is_counter.tv_month-1;
	tm.tm_year = CLOCK->is_counter.tv_year+CHIP0YEAR;
	* secp = unixtime(&tm);
}

write_hardware_clock(sec, msec)
long sec;
int msec;
{
	struct tm *tm;
	struct tm *gmtime();

	tm = gmtime(&sec);
	CLOCK->is_counter.tv_hundredths = msec/10;
	CLOCK->is_counter.tv_seconds = tm->tm_sec;
	CLOCK->is_counter.tv_minutes = tm->tm_min;
	CLOCK->is_counter.tv_hours = tm->tm_hour;
	CLOCK->is_counter.tv_date = tm->tm_mday;
	CLOCK->is_counter.tv_month = tm->tm_mon+1;
	CLOCK->is_counter.tv_year = tm->tm_year-CHIP0YEAR;
}


/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

static int	mon_lengths[2][MONS_PER_YEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int	year_lengths[2] = {
	DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

#ifndef __STDC__
#define const	/* const */
#endif

struct tm *
gmtime(clock)
const time_t *clock;
{
	register struct tm *	tmp;
	register long		days;
	register long		rem;
	register int		y;
	register int		yleap;
	register int *		ip;
	static struct tm	tm;

	tmp = &tm;
	days = *clock / SECS_PER_DAY;
	rem = *clock % SECS_PER_DAY;
	tmp->tm_hour = (int) (rem / SECS_PER_HOUR);
	rem = rem % SECS_PER_HOUR;
	tmp->tm_min = (int) (rem / SECS_PER_MIN);
	tmp->tm_sec = (int) (rem % SECS_PER_MIN);
	tmp->tm_wday = (int) ((EPOCH_WDAY + days) % DAYS_PER_WEEK);
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYS_PER_WEEK;
	y = EPOCH_YEAR;
	if (days >= 0) {
		for ( ; ; ) {
			yleap = isleap(y);
			if (days < (long) year_lengths[yleap])
				break;
			++y;
			days = days - (long) year_lengths[yleap];
		}
	} else do {
		--y;
		yleap = isleap(y);
		days = days + (long) year_lengths[yleap];
	} while (days < 0);
	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int) days;
	ip = mon_lengths[yleap];
	for (tmp->tm_mon = 0; days >= (long) ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long) ip[tmp->tm_mon];
	tmp->tm_mday = (int) (days + 1);
	tmp->tm_isdst = 0;
	return tmp;
}

/*
 * Reverse routine, written by <sater@cs.vu.nl>
 * and undoubtedly already under various names in the public domain
 */

long unixtime(tm)
struct tm *tm;
{
	int year,y;
	int yleap;
	long days;
	register int *ip;
	int m;
	long utime;

	year = tm->tm_year + TM_YEAR_BASE;
	days = 0;
	for (y = EPOCH_YEAR; y < year; y++) {
		yleap = isleap(y);
		days += year_lengths[yleap];
	}
	yleap = isleap(year);
	ip = mon_lengths[yleap];
	for (m = 0; m < tm->tm_mon; m++) {
		days += ip[m];
	}
	days += tm->tm_mday-1;
	utime = days * SECS_PER_DAY;
	utime += tm->tm_hour * SECS_PER_HOUR +
		 tm->tm_min * SECS_PER_MIN +
		 tm->tm_sec /* * SECS_PER_SEC :-) */ ;
	return utime;
}

#endif /* SUN_HW_CLOCK */
