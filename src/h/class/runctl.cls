/*	@(#)runctl.cls	1.2	94/04/06 15:50:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Run server control interface */

#include "ailamoeba.h"
#include "cmdreg.h"

/*
When asked to select a host for a new process, the run server evaluates
the following formula for each host, and chooses a host which
evaluates within EQUIVPERC percent from the highest value:

	(MIN(memleft, MEMTRUNC) + textsize*CACHEMEM) *
				speed * (1 + textsize/CACHETIME)

The following dynamic values are input to the formula:

memleft:	The free memory in the processor minus the size of the
		new program.

textsize:	The size of the program's text segment, IF it is
		believed to be cached; zero otherwise.

speed:		The expected speed at which the program will run;
		computed from the cpu speed and the number of threads
		already running.

The following parameters, settable with run_set_params below, influence
the outcome:

MEMTRUNC:	If memory left is more than MEMTRUNC bytes, pretend only
		MEMTRUNC is left.  This avoids the effect that a host
		with gobs of memory attracts all the small jobs.

CACHETIME:	We expect a speedup of 2 by avoiding having to load
		the text segment for a program with a text segment of
		CACHETIME kbytes.
		XXX Big programs win big here, unfortunately they also
		tend to run longer, which means they get more cradit
		than they deserve.

CACHEMEM:	We expect an average savings of CACHEMEM percent of the
		text size if the text segment is found cached.  CACHEMEM
		represents the expected chance that the text segment is
		already in use by another copy of the same program.

EQUIVPERC:	If two hosts evaluate to values within EQUIVPERC
		percent, they are considered equivalent candidate.  The
		choice between equivalent candidates is made by a random
		generator.

For practical reasons, the values are transferred in kilobytes rather
than in bytes.
*/

class runctl[RUN_FIRST_COM+1 .. RUN_LAST_COM] {
	/* NB: RUN_FIRST_COM is PD_FINDPOOL used by class pooldir */
	
	struct run_params {
		int memtrunc;		/* K bytes */
		int cachetime;		/* K bytes */
		int cachemem;		/* percent */
		int equivperc;		/* percent */
	};
	
	run_set_params(*, var in struct run_params params);
		/* Set the parameters for the formula */
	
	run_get_params(*, out struct run_params params);
		/* Get the parameters for the formula */
	
	run_rescan(*);
		/* Re-read the contents of the pool directory */
};
