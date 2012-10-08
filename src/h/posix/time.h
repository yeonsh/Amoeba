/*	@(#)time.h	1.3	96/02/27 10:35:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Draft ANSI C and Posix 1003.1 compliant time interface */

#ifndef __TIME_H__
#define __TIME_H__

#include "_ARGS.h"

/* These two typedefs are also in <sys/types.h>: */

#ifndef _CLOCK_T
typedef long clock_t;
#define _CLOCK_T
#endif

#ifndef _TIME_T
typedef long time_t;
#define _TIME_T
#endif


#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

/* Posix defines this as the number of clock ticks per second: */
#define CLK_TCK ((clock_t)1000)

/* ANSI C defines this with the same meaning: */
#define CLOCKS_PER_SEC CLK_TCK

struct tm {
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
};

/* ANSI C prototypes */

clock_t		clock	_ARGS((void));
time_t		time	_ARGS((time_t *));		/* Also Posix 4.5.1 */
char *		asctime	_ARGS((const struct tm *));
char *		ctime	_ARGS((const time_t *));
struct tm *	gmtime	_ARGS((const time_t *));
struct tm *	localtime _ARGS((const time_t *));
time_t		mktime	_ARGS((struct tm *));
double		difftime _ARGS((time_t, time_t));
size_t		strftime _ARGS((char *, size_t,
			        const char *, const struct tm *));

/* You may #undef this to get the real function: */
#define difftime(a, b) ( (double) ((a) - (b)) )

/* Posix-only definitions (8.3.2): */

void tzset _ARGS((void));
extern char *tzname[];

#ifndef _POSIX_SOURCE
/* SysV compatibility: */
extern long timezone;
extern int daylight;
#endif

#endif /* __TIME_H__ */
