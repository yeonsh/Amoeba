/*	@(#)timer.c	1.5	96/02/27 10:23:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "stderr.h"
#include "module/tod.h"
#include "module/direct.h"
#include "module/syscall.h"

#include "diag.h"
#include "timer.h"


/*
 * Update my notion of the current time.
 * If there is a TOD_HOST capability in the capability environment present,
 * we lookup "tod" ourselves.
 * Otherwise the time-of-day stub will try to use the TOD capability
 * from the capability environment.
 *
 * Previously, the time was only maintained in minutes to discourage its use.
 * However, some tools (e.g. make) don't work correctly in that case,
 * so this has been changed.
 */
static long Start_time;
static long Current_time; /* Clock updated with TOD server once a minute */
static unsigned long Last_sys_milli; /* To get second granularity */

static void
init_tod_cap()
{
    capability *tod_host;
    errstat     err;

    if ((tod_host = getcap("TOD_HOST")) != NULL) {
	static capability todcap;

	if ((err = dir_lookup(tod_host, "tod", &todcap)) == STD_OK) {
	    tod_setcap(&todcap);
	    message("initialized tod cap");
	} else {
	    scream("could not lookup tod cap (%s)", err_why(err));
	}
    }
}

void
time_update()
{
    static int first_time = 1;
    long       seconds;
    int        millisecs, tz, dst;
    int        try, nr_tries;
    errstat    err;
    
#ifdef AMOEBA
    /* If we try to get the time for the first time, we demand it to succeed,
     * to make sure that we started with a good time-of-day capability.
     */
    if (first_time) {
	init_tod_cap();
    }

    nr_tries = first_time ? 3 : 1;
    for (try = 0; try < nr_tries; try++) {
	if ((err = tod_gettime(&seconds, &millisecs, &tz, &dst)) == STD_OK) {
	    break;
	}
	init_tod_cap();
    }

    Last_sys_milli = sys_milli();
#else
    time(&seconds);
    err = STD_OK;
#endif

    if (err == STD_OK) {
	Current_time = seconds;
	if (first_time) {
	    first_time = 0;
	    Start_time = Current_time;
	}
    } else {
	if (first_time) {
	    panic("cannot get the time (%s)", err_why(err));
	} else {
	    scream("cannot get the time (%s)", err_why(err));
	}
    }
}

long
sp_time_start()
{
    return Start_time;
}

#ifdef SOAP_GROUP

/* In the group version, the time must be kept consitent among the members.
 * The function sp_time(), which returns this consistent value, is defined
 * elsewhere.
 */
long
sp_mytime()
{
    return Current_time;
}

#else

long
sp_time()
{
    unsigned long now = sys_milli();

    if (now > Last_sys_milli) {
	return Current_time + (now - Last_sys_milli) / 1000;
    } else {
	/* sys_milli wrapped around */
	return Current_time;
    }
}

#endif
