/*	@(#)cron_stub.c	1.2	94/04/07 15:57:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	"amtools.h"
#include	<sys/types.h>
#include	"time.h"
#include	"sak.h"
#include	"cron_stub.h"

#define skipdigit(p)	while (isdigit (*(p))) (p)++;
#define skipspace(p)	while (*(p) == ' ') (p)++;

#define	NR_WDAYS	7
#define	YEAR_BASE	1900
#define	FALSE		0
#define	TRUE		1

int cronoptind;
int cronopterr = 1;

#ifdef DEBUG
extern int debug;
#endif


/* getfield ()
 * convert an ascii crontab field to a schedule entry
 * space for psched will be malloced
 * field	in	ascii crontab field
 * psched	out	pointer to schedule entry
 * return		STD_OK if succes else error status
 */
static errstat getfield (field, psched)
char *field;
int8 **psched;
{
	char *sec, *p;
	int i;
	size_t size;
	int8 type;

	if (! strcmp (field, "*")) {
		type = ANY;
		size = 2;
	} else {
		if (! isdigit (*field)) return (STD_ARGBAD);

		sec = field;
		skipdigit (sec);
		skipspace (sec);

		if (*sec == ',') {
			type = LIST;
			size = 3;
			p = sec;
			while (*p == ',') {
				p++;
				skipspace (p);
				if (! isdigit (*p))
					return (STD_ARGBAD);
				skipdigit (p);
				skipspace (p);
				size++;
			}
		} else if (*sec == '-') {
			type = RANGE;
			size = 4;
		} else if (*sec == '\0') {
			type = VALUE;
			size = 3;
		} else
			return (STD_ARGBAD);
	}

	if ((*psched = (int8 *) malloc (size * sizeof (int8))) == (int8 *) 0)
		return (STD_NOMEM);

	(*psched)[0] = type;
	(*psched)[size - 1] = DELIMITER;
	switch (type) {
	case VALUE:
		(*psched)[1] = atoi (field);
		break;
	case RANGE:
	case LIST:
		(*psched)[1] = atoi (field);
		i = 2;
		do {
			sec++;
			skipspace (sec);
			if (! isdigit (*sec))
				return (STD_ARGBAD);
			(*psched)[i++] = atoi (sec);
			skipdigit (sec);
			skipspace (sec);
		} while (*sec == ',' && type == LIST);
		if (*sec != '\0')
			return (STD_ARGBAD);
		break;
	}

	return (STD_OK);
}

/* convert_cronline ()
 * convert a ascii crontab entry to a schedule.
 * argv must contain >= 5 fields (argc>= 4)
 * argv[argc]:     0       1         2         3         4
 * fields:	minutes  hours  day-of-month  month  day-of-weeks
 * each field can be:
 *	*		any
 *	x		a value
 *	x,x,..		a list of values
 *	x-y		a range of values
 *
 * There two ways of specifying a day : day-of-week or day-of-month, month
 * if one is set to any and the other is not, the former will be reset
 * to none, meaning schedule only on the later day. If both are set to any
 * both are left unchanged, meaning schedule on any day. If both are not
 * set to any both are left unchanged, meaning schedule on whichever day
 * comes first.
 *
 * The space for the schedule will malloced.
 * if return status is not STD_OK space for the schedule will NOT have been 
 * malloced.
 *
 * If a error occurs a error message will be printed if cronopterr != 0
 * on exit cronoptind will be point t the first unparsed argument in argv
 * (always 5 if succes)
 *
 * argc		in	# arguments in argv
 * argv		in	crontab fields in ascii
 * psched	out	pointer to schedule
 * return		STD_OK if succes else error status
 */
errstat convert_cronline (argc, argv, psched)
int argc;
char *argv[];
int8 ***psched;
{
	int i,j;
	errstat err;
	int8 **sched;

	cronoptind = 0;

	*psched = (int8 **) malloc (MAX_SPEC * sizeof (int8 *));
	if (*psched == (int8 **) 0) {
		if (cronopterr)
			fprintf (stderr,
			  "convert_cronline: malloc failed. Out of Memory\n");
		return (STD_NOMEM);
	}

	sched = *psched;

	for (i = 0; i < YEAR; i++) {
		sched[i] = (int8 *) 0;
		if (cronoptind >= argc) {
			err = STD_ARGBAD;
			if (cronopterr)
				fprintf (stderr, "Bad # of arguments\n");
			break;
		} else if ((err = getfield (argv[cronoptind], &sched[i]))
								!= STD_OK) {
			if (cronopterr) {
				if (err == STD_NOMEM)
					fprintf (stderr,
					      "Malloc failed. Out of Memory\n");
				else
					fprintf (stderr,
					"Bad field specification `%s'\n",
					argv[cronoptind]);
			}
			break;
		}
		cronoptind++;
		if (i == MONTH || i == WDAY) {
			for (j = 1; sched[i][j] != DELIMITER; j++)
				if (i == WDAY) {
					if (sched[i][j] == 7)
						sched[i][j] = 0;
				} else 
					sched[i][j]--;
		}
	}

	if (i < YEAR) { /* something failed, free all malloc lines */
		do {
			if (sched[i] != (int8 *) 0) {
				free ((_VOIDSTAR) sched[i]);
				sched[i] = (int8 *) 0;
			}
		} while (--i >= 0);
		free ((_VOIDSTAR) sched);
		return (err);
	}

	if (sched[WDAY][0] == ANY &&
			(sched[MDAY][0] != ANY || sched[MONTH][0] != ANY))
		sched[WDAY][0] = NONE;

	if (sched[WDAY][0] != ANY &&
			(sched[MDAY][0] == ANY && sched[MONTH][0] == ANY)) {
		sched[MDAY][0] = NONE;
		sched[MONTH][0] = NONE;
	}

	/* trick to get sched[YEAR] to ANY */
	err = getfield ("*", &sched[YEAR]);

	if (err == STD_NOMEM && cronopterr)
			fprintf (stderr, "Malloc failed. Out of Memory\n");
#ifdef DEBUG
	if (debug > 0) speakschedule (sched);
#endif
	return (err);
}

#ifdef DEBUG
static char *names[] = {
	/* DAYNAMES */
	"sunday", "monday", "tuesday", "wednesday",
	"thursday", "friday", "saturday",
	/* MONTHNAMES */
	"january", "february", "march", "april", "may", "june",
	"july", "august", "september", "october", "november", "december",
	(char *) 0
};

static void printval (val, spec)
int8 val;
int spec;
{
	switch (spec) {
	case MINU:
	case HOUR:
	case MDAY:  printf ("%d", (int) val); break;
	case YEAR:  printf ("%d", (int) val + YEAR_BASE); break;
	case WDAY:  printf ("%s", names[val]); break;
	case MONTH: printf ("%s", names[val + NR_WDAYS]);
	}
}

speakschedule (sched)
int8 **sched;
{
	int i, j;

	for (i = 0; i < MAX_SPEC; i++) {
		switch (i) {
		case MINU:  printf ("Minutes      : "); break;
		case HOUR:  printf ("Hours        : "); break;
		case WDAY:  printf ("Week days    : "); break;
		case MDAY:  printf ("Day of month : "); break;
		case MONTH: printf ("Month        : "); break;
		case YEAR:  printf ("Year         : "); break;
		}
		switch (sched[i][0]) {
		case NONE:
			printf ("never"); break;
		case ANY:
			printf ("any"); break;
		case VALUE:
			printval (sched[i][1], i); break;
		case RANGE:
			printval (sched[i][1], i);
			printf (" - ");
			printval (sched[i][2], i); break;
		case LIST:
			printval (sched[i][1], i);
			for (j = 2; sched[i][j] != DELIMITER; j++) {
				printf (", ");
				printval (sched[i][j], i);
			}
			break;
		}
		printf ("\n");
	}
}
#endif
