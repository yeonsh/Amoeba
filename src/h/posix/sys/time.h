/*	@(#)time.h	1.2	94/04/06 16:57:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TIMEVAL__
#define __TIMEVAL__

/* Not in any sense compliant with anything, but currently needed to compile
 * libajax under amoeba.
 */

struct timeval {
        long    tv_sec;
        long    tv_usec;
};
struct timezone {
        int     tz_minuteswest;
        int     tz_dsttime;
};
#endif /* __TIMEVAL__ */
