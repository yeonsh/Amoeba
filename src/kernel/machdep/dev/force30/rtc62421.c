/*	@(#)rtc62421.c	1.3	94/04/06 09:06:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Real-time-clock
 * We peek in here to get some random numbers
 */

#include "map.h"
#include "randseed.h"

#ifndef NORANDOM
struct rtc {
	char	rtc_sec1;
	char	rtc_sec10;
	char	rtc_min1;
	char	rtc_min10;
	char	rtc_hour1;
	char	rtc_hour10;
	char	rtc_day1;
	char	rtc_day10;
	char	rtc_mon1;
	char	rtc_mon10;
	char	rtc_year1;
	char	rtc_year10;
	char	rtc_week;
	char	rtc_conD;
	char	rtc_conE;
	char	rtc_conF;
};

void
rtc_init() {
	struct rtc *dev= (struct rtc *) DVADR_RTC62421;

	randseed((unsigned long) dev->rtc_sec1  , 3, RANDSEED_INVOC);
	randseed((unsigned long) dev->rtc_sec10 , 2, RANDSEED_INVOC);
	randseed((unsigned long) dev->rtc_min1  , 3, RANDSEED_INVOC);
	randseed((unsigned long) dev->rtc_min10 , 2, RANDSEED_INVOC);
	randseed((unsigned long) dev->rtc_hour1 , 3, RANDSEED_INVOC);
	randseed((unsigned long) dev->rtc_hour10, 1, RANDSEED_INVOC);
}
#endif
