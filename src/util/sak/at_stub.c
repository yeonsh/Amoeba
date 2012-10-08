/*	@(#)at_stub.c	1.2	94/04/07 15:57:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	<sys/types.h>
#include	"amtools.h"
#include	"time.h"
#include	"sak.h"
#include	"at_stub.h"


#define	NR_WDAYS	7
#define	YEAR_BASE	1900
#define	FALSE		0
#define	TRUE		1

int atoptind;
int atopterr = 1;	/*  controlls printing of error messages if set to 0
			 * it will not print error messages */

#ifdef DEBUG
extern int debug;
#endif

extern char * progname;

static char *names[] = {
	/* DAYNAMES */
	"sunday", "monday", "tuesday", "wednesday",
	"thursday", "friday", "saturday",
	/* MONTHNAMES */
	"january", "february", "march", "april", "may", "june",
	"july", "august", "september", "october", "november", "december",
	(char *) 0
};

/* Local error numbers when getting time and date */
#define	AT_OK			0
#define	AT_TOOLATE		1
#define	AT_BADTIME		2
#define	AT_BADDATE		3
#define	AT_MIN_OOR		4
#define	AT_HOUR_OOR		5
#define	AT_DAY_OOR		6

static char *sak_errs [] = {
	"",
	"Time/date is in the past",
	"Bad time conversion",
	"Bad date conversion",
	"Minute value out of range",
	"Hour value out of range",
	"Day value out of range",
};

/* leap_year ()
 *
 * year		in	year to be checked
 * return 		TRUE if year is a leap year
 */
static int8 leap_year (year)
int year;
{
	year += YEAR_BASE;
	if (year % 4 == 0) {
		if (year % 100 == 0) {
			if (year % 400 == 0) return (TRUE);
			else return (FALSE);
		} else return (TRUE);
	}
	return (FALSE);
}

/* last_day_of_month ()
 *
 * pmatch	in	tm struct with the current date
 * return		last day in the month specified by pmatch
 */
static int last_day_of_month (pmatch)
struct tm *pmatch;
{
	static int days_of_month[] = {
		 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if (pmatch->tm_mon == 1 && leap_year (pmatch->tm_year))
		return (29);
	return (days_of_month[pmatch->tm_mon]);
}

/* get_time ()
 * convert a given time to a houres and minutes
 * knows about special keywords : "now", "midnight" and "noon"
 * knows about "am" and "pm" postfix
 * 24 hour clock is used (unless am or pm is specified)
 * time can be specified as : hh:mm, h:mm, h , hh, hh:m, h:m, hhm, hhmm
 * argc		in	# arguments in argv
 * argv		in	arguments specifying the time in ascii
 * pnext	out	fields tm_min and tm_hour set to the time specified
 *			in argv
 * return 		STD_OK or error status
 */

static int get_time (argc, argv, pnext)
int argc;
char **argv;
struct tm *pnext;
{
	char *tline;

	if (atoptind >= argc)
		return (AT_BADTIME);

	tline = argv[atoptind];

	if (! strcmp (tline, "now")) {
		atoptind++;
		return (AT_OK);
	}

	pnext->tm_min = 0;

	if (! strcmp (tline, "noon")) {
		pnext->tm_hour = 12;
		atoptind++;
		return (AT_OK);
	} else if (! strcmp (tline, "midnight")) {
		pnext->tm_hour = 0;
		atoptind++;
		return (AT_OK);
	}

	if (! isdigit (*tline))
		return (AT_BADTIME);

	pnext->tm_hour = *tline++ - '0';
	if (isdigit (*tline)) {
		pnext->tm_hour *= 10;
		pnext->tm_hour += *tline++ - '0';
		if (pnext->tm_hour > 23)
			return (AT_HOUR_OOR);
	}
	if (*tline == ':') tline++;
	if (isdigit (*tline)) {
		pnext->tm_min = *tline++ - '0';
		if (! isdigit (*tline))
			return (AT_BADTIME);
		pnext->tm_min *= 10;
		pnext->tm_min += *tline++ - '0';
		if (pnext->tm_min > 59)
			return (AT_MIN_OOR);
	}

	if (*tline == '\0' && atoptind + 1 < argc &&
			(!strcmp (argv[atoptind + 1], "am") ||
			 !strcmp (argv[atoptind + 1], "pm"))) {
		atoptind++;
		tline = argv[atoptind];
	}

	if (! strcmp (tline, "am")) {
		if (pnext->tm_hour >  12) {
			atoptind++;
			return (AT_OK);
		} else
			return (AT_BADTIME);
	} else if (! strcmp (tline, "pm")) {
		if (pnext->tm_hour >  12)
			return (AT_BADTIME);
		if (pnext->tm_hour == 12)
			pnext->tm_hour = 0;
		else
			pnext->tm_hour += 12;
		atoptind++;
		return (AT_OK);
	}

	if (*tline != '\0')
		return (AT_BADTIME);
	else {
		atoptind++;
		return (AT_OK);
	}
}

/* compare to tm structs to see if first time is before or after the
 * second time. (if first time later then second return TRUE)
 */
static int8 timepast (pnow, pnext)
struct tm *pnow, *pnext;
{
	if (pnow->tm_year < pnext->tm_year)		return (FALSE);
	else if (pnow->tm_year > pnext->tm_year)	return (TRUE);

	if (pnow->tm_mon < pnext->tm_mon)		return (FALSE);
	else if (pnow->tm_mon > pnext->tm_mon)		return (TRUE);

	if (pnow->tm_mday < pnext->tm_mday)		return (FALSE);
	else if (pnow->tm_mday > pnext->tm_mday)	return (TRUE);

	if (pnow->tm_hour < pnext->tm_hour)		return (FALSE);
	else if (pnow->tm_hour > pnext->tm_hour)	return (TRUE);

	if (pnow->tm_min <= pnext->tm_min)		return (FALSE);
	else 						return (TRUE);
}

static void add_years (pnext, nr)
struct tm *pnext;
int nr;
{
	pnext->tm_year += nr;
}

static void add_months (pnext, nr)
struct tm *pnext;
int nr;
{
	nr += pnext->tm_mon;
	pnext->tm_mon = nr % 12;
	if (nr > 11) add_years (pnext, nr / 12);
}

static void add_days (pnext, nr)
struct tm *pnext;
int nr;
{
	int ldom;

	nr += pnext->tm_mday;
	while (nr > (ldom = last_day_of_month (pnext))) {
		nr -= ldom;
		add_months (pnext, 1);
	}
	pnext->tm_mday = nr;
}

static void add_hours (pnext, nr)
struct tm *pnext;
int nr;
{
	nr += pnext->tm_hour;
	pnext->tm_hour = nr % 24;
	if (nr > 23) add_days (pnext, nr / 24);
}

static void add_minutes (pnext, nr)
struct tm *pnext;
int nr;
{
	nr += pnext->tm_min;
	pnext->tm_min = nr % 60;
	if (nr > 59) add_hours (pnext, nr / 60);
}

/* get_date ()
 * convert a ascii date (specified in argc, argv) into a tm struct
 * knows about special keywords : "today" and "tomorrow"
 * knows about postfix : + # <"years"|"months"|"weeks"|"days"|"minutes"|"hours">
 * date can be specified as : <monthname> <day-of-month> or <weekday>
 * if no date is specified and the time (specified in pnext) is
 * earlier then current time (specfied in pnext) tomorrow is assumed.
 * argc		in	# arguments in argv
 * argv		in	arguments specifying the date in ascii
 * pnow		in	tm struct specifying the current date/time
 * pnext	out	date fields set to the date specified
 *			in argv
 * return 		STD_OK or error status
 */

static int get_date (argc, argv, pnow, pnext)
int argc;
char **argv;
struct tm *pnow, *pnext;
{
	int i;
	size_t size;
	int nr;
	char *dline;

	if (atoptind  >= argc) {
		if (timepast (pnow, pnext)) add_days (pnext, 1);
		return (AT_OK);
	}

	if (*argv[atoptind] == '+') {
		if (timepast (pnow, pnext)) add_days (pnext, 1);
	} else if (!strcmp (argv[atoptind], "today")) {
		atoptind++;
	} else if (!strcmp (argv[atoptind], "tomorrow")) {
		add_days (pnext, 1);
		atoptind++;
	} else {
		if ((size = strlen (argv[atoptind])) < 3) {
			if (timepast (pnow, pnext)) add_days (pnext, 1);
			return (AT_OK);
		}

		for (i = 0; names[i] != (char *) 0; i++) {
			if (! strncmp (names[i], argv[atoptind], size))
				break;
		}

		if (names[i] == (char *) 0) {
			if (timepast (pnow, pnext)) add_days (pnext, 1);
			return (AT_OK);
		}

		atoptind++;

		if (i < NR_WDAYS) {
			nr = i - pnow->tm_wday;
			if (nr == 0 && timepast (pnow, pnext))
				nr = 7;
			else if (nr < 0)
				nr += 7;
			add_days (pnext, nr);
		} else {
			i -= NR_WDAYS;
			if (atoptind >= argc || ! isdigit (*argv[atoptind]))
				return (AT_BADDATE);

			pnext->tm_mday = atoi (argv[atoptind]);
			pnext->tm_mon = i;

			atoptind++;

			if (atoptind < argc && isdigit (*argv[atoptind])) {
				pnext->tm_year = atoi (argv[atoptind]) -
						 YEAR_BASE;
				atoptind++;
			} else if (timepast (pnow, pnext))
				add_years (pnext, 1);

			if (pnext->tm_mday <= 0 ||
			    pnext->tm_mday > last_day_of_month (pnext)) {
				return (AT_DAY_OOR);
			}
		}
	}

	if (atoptind < argc && *argv[atoptind] == '+') {
		if (isdigit (argv[atoptind][1]))
			nr = atoi (&argv[atoptind][1]);
		else {
			if (argv[atoptind][1] != '\0' ||
					    ++atoptind >= argc ||
					    ! isdigit (*argv[atoptind]))
				return (AT_BADDATE);
			nr = atoi (argv[atoptind]);
		}
		if (++atoptind >= argc)
			return (AT_BADDATE);

		dline = argv[atoptind];
		if (! strncmp (dline, "minute", 6)) {
			add_minutes (pnext, nr);
			dline += 6;
		} else if (! strncmp (dline, "hour", 4)) {
			add_hours (pnext, nr);
			dline += 4;
		} else if (! strncmp (dline, "day", 3)) {
			add_days (pnext, nr);
			dline += 3;
		} else if (! strncmp (dline, "week", 4)) {
			add_days (pnext, nr * 7);
			dline += 4;
		} else if (! strncmp (dline, "month", 5)) {
			add_months (pnext, nr);
			dline += 5;
		} else if (! strncmp (dline, "year", 4)) {
			add_years (pnext, nr);
			dline += 4;
		} else
			return (AT_BADDATE);

		if (*dline == 's') dline++;
		if (*dline != '\0')
			return (AT_BADDATE);

		atoptind++;
	}

	return (AT_OK);
}

/* translate a ascii time and date in to a tm struct
 * date is optional
 * if time/date specified is earlier then current time/date return an error
 * argc		in	# arguments in argv
 * argv		in	arguments specifying the date in ascii
 * pnow		in	tm struct specifying the current date/time
 * pnext	out	date/time fields set to the date/time specified
 *			in argv
 * return 		STD_OK or error status
 */
static int get_time_and_date (argc, argv, pnow, pnext)
int argc;
char **argv;
struct tm *pnow, *pnext;
{
	int err;

	*pnext = *pnow;
	atoptind = 0;

	if ((err = get_time (argc, argv, pnext)) != STD_OK)
		return (err);

	if ((err = get_date (argc, argv, pnow, pnext)) != STD_OK)
		return (err);

	if (timepast (pnow, pnext))
		return (AT_TOOLATE);

	return (AT_OK);
}

/* convert_atline ()
 * convert ascii date/time (specified in argc, argv) into a schedule
 * suitable as input for the sak server(L).
 * the global variable atoptind will point to the first unparsed argument
 * in argv. If an error occurs a error message will be printed depending
 * on the value of the global variable atopterr (default = 1 = print error
 * message). The space for the schedule will created using malloc
 * argc		in	# arguments in argv
 * argv		in	time and optional date to be converted
 * psched	out	schedule suitable for sak(L)
 * return 		STD_OK or error status
 */

errstat convert_atline (argc, argv, psched)
int argc;
char *argv[];
int8 ***psched;
{
	time_t clock;
	struct tm *pnow, next;
	int i;
	int err;
	int8 **sched;

	*psched = (int8 **) malloc (MAX_SPEC * sizeof (int8 *));
	if (*psched == (int8 **) 0) {
		if (atopterr)
			fprintf (stderr, "%s: Malloc failed. Out of Memory\n", progname);
		return (STD_NOMEM);
	}

	sched = *psched;

	time (&clock);
	pnow = localtime (&clock);

	if ((err = get_time_and_date (argc, argv, pnow, &next)) != AT_OK) {
		if (atopterr)
			fprintf (stderr, "%s: %s\n", progname, sak_errs[err]);
		return (STD_ARGBAD);
	}
	
#ifdef DEBUG
	if (debug > 0) {
		time_t clock;

		printf ("Spec      : '");
		for (i = 0; i < atoptind; i++)
			printf ("%s%s", argv[i], i + 1 < atoptind ? " " : "'");
		printf ("\nNow       : %s", asctime (pnow));
		next.tm_isdst = -1;
		clock = mktime (&next);
		printf ("Spec date : %s", ctime (&clock));
	}
#endif
	for (i = 0; i < MAX_SPEC; i++) {
		if ((sched[i] = (int8 *) malloc (3 * sizeof (int8)))
								== (int8 *) 0)
			break;
		sched[i][0] = VALUE;
		sched[i][2] = DELIMITER;
	}

	if (i < MAX_SPEC) { /* something failed, free all malloc lines */
		do {
			if (sched[i] != (int8 *) 0) {
				free ((_VOIDSTAR) sched[i]);
				sched[i] = (int8 *) 0;
			}
		} while (--i >= 0);
		free ((_VOIDSTAR) sched);
		if (atopterr)
			fprintf (stderr, "Malloc failed. Out of Memory\n");
		return (STD_NOMEM);
	}

	sched[WDAY][0] = NONE;
	sched[WDAY][1] = DELIMITER;	/* malloc one to much. So what.. */

	sched[MINU][1]  = next.tm_min;
	sched[HOUR][1]  = next.tm_hour;
	sched[MDAY][1]  = next.tm_mday;
	sched[MONTH][1] = next.tm_mon;
	sched[YEAR][1]  = next.tm_year;

	return (STD_OK);
}
