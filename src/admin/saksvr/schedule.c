/*	@(#)schedule.c	1.2	94/04/06 11:53:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* schedule.c
 *
 * calculate a time according to specified schedule and current time
 */
#include	"saksvr.h"

#define	YEAR_BASE	1900
#define	OK		0
#define	OVERFLOW	1
#define	NEVER		2

#define thisyear(x,y)		(match ((x)->schedule[YEAR],  (y)->tm_year))
#define thismonth(x,y)		(match ((x)->schedule[MONTH], (y)->tm_mon))
#define thismday(x,y)		(match ((x)->schedule[MDAY],  (y)->tm_mday))
#define thiswday(x,y)		(match ((x)->schedule[WDAY],  (y)->tm_wday))
#define	thisday(x,y)		((thismday (x, y) && thismonth (x, y) && \
				  thisyear (x, y)) || thiswday (x, y))
#define thishour(x,y)		(match ((x)->schedule[HOUR],  (y)->tm_hour))
#define thisminute(x,y)		(match ((x)->schedule[MINU],   (y)->tm_min))

mutex tm_sem;			/* used to protect localtime (3) lib routines
				 * from multiple access, since localtime (3)
				 * uses static data
				 */

int8 max_spec[] = { 59, 23, 31, 11, 6, 0x7f };
int8 min_spec[] = { 0, 0, 1, 0, 0, 0 };


static int8 match (list, now)
int8 list[];
int now;
{
	int i;

	switch (list[0]) {
	case ANY:
		return (TRUE);
	case VALUE:
		return (list[1] == now);
	case LIST:
		for (i = 1; list[i] != DELIMITER; i++)
			if (list[i] == now)
				return (TRUE);
		return (FALSE);
	case RANGE:
		return (now >= list[1] && now <= list[2]);
	default: /* HuH ?? */
	case NONE:
		return (FALSE);
	}
}

static int nextmatch (list, pnext, first, last)
int8 list[];
int *pnext, first, last;
{
	int i;

	switch (list[0]) {
	case ANY:
		if (*pnext == last) {
			*pnext = first;
			return OVERFLOW;
		} else {
			++(*pnext);
			return OK;
		}
	case VALUE:
		if (*pnext >= list[1]) {
			*pnext = list[1];
			return OVERFLOW;
		} else {
			*pnext = list[1];
			return OK;
		}
	case LIST:
		for (i = 1; list[i] != DELIMITER; i++)
			if (*pnext < list[i]) {
				*pnext = list[i];
				return OK;
			}
		*pnext = list[1];
		return OVERFLOW;
	case RANGE:
		if (*pnext < list[1]) {
			*pnext = list[1];
			return OK;
		} else if (*pnext < list[2]) {
			++(*pnext);
			return OK;
		}
		*pnext = list[1];
		return OVERFLOW;
	default: /* HuH ?? */
	case NONE:
		return NEVER;
	}
}

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

static int8 getnextyear (pjob, pmatch)
struct sak_job *pjob;
struct tm *pmatch;
{
	int stat;

	stat = nextmatch (pjob->schedule[YEAR], &pmatch->tm_year,
			  min_spec[YEAR], max_spec[YEAR]);
		/* OVERFLOW on year means NEVER */
	if (stat != OK)	return (FALSE);
	else		return (TRUE);
}

static int8 getnextmonth (pjob, pmatch)
struct sak_job *pjob;
struct tm *pmatch;
{
	int stat;

	stat = nextmatch (pjob->schedule[MONTH], &pmatch->tm_mon,
			  min_spec[MONTH], max_spec[MONTH]);
	if (stat == NEVER) 		return (FALSE);
	else if (stat == OVERFLOW)	return (getnextyear (pjob, pmatch));
	else				return (TRUE);
}

static int8 getnextmday (pjob, pmatch)
struct sak_job *pjob;
struct tm *pmatch;
{
	int stat;

	stat = nextmatch (pjob->schedule[MDAY], &pmatch->tm_mday,
			  min_spec[MDAY], last_day_of_month (pmatch));

	if (stat == NEVER)		return (FALSE);
	else if (stat == OVERFLOW)	return (getnextmonth (pjob, pmatch));
	else				return (TRUE);
}

static int8 getnextday (pjob, pmatch)
struct sak_job *pjob;
struct tm *pmatch;
{
	int last_day;

	if (pjob->schedule[WDAY][0] == NONE) {
		/* looking for a specific date (day/month/year) */
		if (! getnextmday (pjob, pmatch))
			return (FALSE);
		if (! thisyear (pjob, pmatch)) {
			pmatch->tm_mon = min_spec[MONTH];
			pmatch->tm_mday = min_spec[MDAY];
			if (! getnextyear (pjob, pmatch))
				return (FALSE);
		}
		if (! thismonth (pjob, pmatch)) {
			pmatch->tm_mday = min_spec[MDAY];
			if (! getnextmonth (pjob, pmatch))
				return (FALSE);
		}
		if (! thismday (pjob, pmatch)) {
			if (! getnextmday (pjob, pmatch))
				return (FALSE);
		}
	}
	else {
		/* looking for either a specific date (day/month/year) or
		 * a day of the week (Sunday/Monday/...) which ever comes
		 * first.
		 */
		/* We don't know which match comes first : day of week or
		 * day of month, but since there are only 7 days in a week
		 * and one of them will match dow, it will just keep
		 * incrementing (max 6 times) the dow and dom till one of
		 * them matches.
		 */
		last_day = last_day_of_month (pmatch);
		do {
			if (pmatch->tm_wday == max_spec[WDAY])
				pmatch->tm_wday = min_spec[WDAY];
			else
				++pmatch->tm_wday;
			if (pmatch->tm_mday == last_day) {
				if (pmatch->tm_mon == max_spec[MONTH]) {
					pmatch->tm_mon = min_spec[MONTH];
					pmatch->tm_year++;
				} else
					pmatch->tm_mon++;
				/* cannot cycle more then 6 days !! */
				/* so next statement is unneccesary, but still
				 * maybe someday sombody invents a month of
				 * less then 6 days :-)
				 */
				last_day = last_day_of_month (pmatch);
			} else
				pmatch->tm_mday++;
		} while (! thisday (pjob, pmatch));
	}

	return (TRUE);
}

static int8 getnexthour (pjob, pmatch)
struct sak_job *pjob;
struct tm *pmatch;
{
	int stat;

	stat = nextmatch (pjob->schedule[HOUR], &pmatch->tm_hour, 
			  min_spec[HOUR], max_spec[HOUR]);

	if (stat == NEVER)		return (FALSE);
	else if (stat == OVERFLOW)	return (getnextday (pjob, pmatch));
	else				return (TRUE);
}

static int8 getnextminute (pjob, pmatch)
struct sak_job *pjob;
struct tm *pmatch;
{
	int stat;

	stat = nextmatch (pjob->schedule[MINU], &pmatch->tm_min,
			  min_spec[MINU], max_spec[MINU]);

	if (stat == NEVER) 		return (FALSE);
	else if (stat == OVERFLOW)	return (getnexthour (pjob, pmatch));
	else				return (TRUE);
}

/* if any of the getnext functions fails the info in the nexttm will
 * be useless
 */

/* schedule assumes job_list_sem is locked (or pjob not on job_list (e.g new))
 */
int8 schedule (pjob, now)
struct sak_job *pjob;
time_t now;
{
	time_t realtime;
	struct tm nexttm;
	int8 failed;

	(void) time (&realtime);

	mu_lock (&tm_sem);
		nexttm = * ((struct tm *) localtime (&now));
		nexttm.tm_sec = 0;
	mu_unlock (&tm_sem);

	failed = ! getnextminute (pjob, &nexttm);

	if (!failed && ! thisday (pjob, &nexttm)) {
		nexttm.tm_hour = min_spec[HOUR];
		nexttm.tm_min = min_spec[MINU];
		failed = ! getnextday (pjob, &nexttm);
	}
	if (!failed && ! thishour (pjob, &nexttm)) {
		nexttm.tm_min = min_spec[MINU];
		failed = ! getnexthour (pjob, &nexttm);
	}
	if (!failed && ! thisminute (pjob, &nexttm))
		failed = ! getnextminute (pjob, &nexttm);

	if (failed) {
		if (pjob->opt.catchup) {
			pjob->delayed = TRUE;
			pjob->executiondate = realtime;
			return (TRUE);
		} else
			return (FALSE);
	} else {
		nexttm.tm_isdst = -1; /* let mktime figure out if isdst */
		mu_lock (&tm_sem);
			pjob->executiondate = mktime (&nexttm);
		mu_unlock (&tm_sem);
	}

	if ((pjob->executiondate + 60) < realtime) {
		pjob->delayed = TRUE;
		pjob->executiondate = realtime;
	}

#ifdef DEBUG
	if (debug > 1) {
		mu_lock (&tm_sem);
			printf ("Scheduled job named %s for %s",
					pjob->opt.name,
					ctime (&pjob->executiondate));
		mu_unlock (&tm_sem);
	}
#endif
	return (TRUE);
}

static int8 valid_sched_line (schedl, first, last)
int8 *schedl, first, last;
{
	int size, i, j;
	int8 tmp;

	for (size = 0; schedl[size] != DELIMITER; size++)
		;

	size--;
	switch (schedl[0]) {
	case NONE:
	case ANY:
		return (size == 0);
	case VALUE:
		if (size != 1)
			return (FALSE);
		else
			return (schedl[1] >= first && schedl[1] <= last);
	case LIST:
		if (size < 1 || size > MAX_SPEC_SIZE - 2)
			return (FALSE);
			/* can't be very long list so do a easy but slow
			 * sort
			 */
		for (i = 1; i < size; i++)
			for (j = i + 1; j < size + 1; j++) {
				if (schedl[j] == schedl[i]) {
					/* delete doubles */
					schedl[j] = schedl[size];
					schedl[size] = DELIMITER;
					size--;
					continue;
				}

				if (schedl[j] < schedl[i]) {
					tmp = schedl[j];
					schedl[j] = schedl[i];
					schedl[i] = tmp;
				}
			}
#ifdef DEBUG
		if (debug > 2) {
			printf ("Sorted list : ");
			for (i = 1; i < size; i++)
				printf ("%d ", (int) schedl[i]);
		}
#endif
		

		return (schedl[1] >= first && schedl[size] <= last);
	case RANGE:
		if (size != 2)
			return (FALSE);
		if (schedl[1] < first || schedl[1] > last ||
						schedl[1] > schedl[2])
			return (FALSE);
		if (schedl[2] < first || schedl[2] > last)
			return (FALSE);
		return (TRUE);
	default:
		return (FALSE);
	}
}

int8 valid_sched (sched)
int8 **sched;
{
	int i;

	for (i = 0; i < MAX_SPEC; i++) {
		if (! valid_sched_line (sched[i], min_spec[i], max_spec[i]))
			return (FALSE);
	}

	return (TRUE);
}
