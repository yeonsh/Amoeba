/*	@(#)watchdog.c	1.5	96/02/27 10:23:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "monitor.h"
#include "module/stdcmd.h"
#include "module/cap.h"

#include "global.h"
#include "caching.h"
#include "filesvr.h"
#include "dirfile.h"
#include "intentions.h"
#include "misc.h"
#include "sp_impl.h"
#include "superfile.h"
#include "main.h"

#ifdef SOAP_GROUP
#include "modlist.h"
#endif

#define MAX_WATCHDOG	100	 /* Max number of directory Bullet files to
				  * touch or replicate on each call to watchdog
				  */

static void
do_watchdog(obj)
objnum obj;
{
    int	       good_file[NBULLETS];
    capability filecaps[NBULLETS];
    capability origcaps[NBULLETS];  /* initial value of filecaps */
    capability bulcaps[NBULLETS];
    errstat    err;
#ifndef SOAP_GROUP
    int        j;
#endif

    get_dir_caps(obj, origcaps);
    get_dir_caps(obj, filecaps);
    fsvr_get_svrs(bulcaps, NBULLETS);
    
    err = dirf_touch(obj, filecaps, good_file);

#ifdef SOAP_GROUP
    if (err != STD_OK) {
	/* Touch failed: bullet server dead or file invalid?
	 * Try to restore it when we have the directory in cache.
	 */
	if (in_cache(obj) && fsvr_nr_avail() > 0) {
	    if ((err = ml_write_dir(obj)) == STD_OK) {
		message("watchdog: recovered directory %ld", (long) obj);
	    } else {
		scream("watchdog: could not recover directory %ld (%s)",
		       (long) obj, err_why(err));
	    }
	} else {
	    scream("watchdog: could not touch dirfile for %ld (%s)",
		   (long) obj, err_why(err));
	}
    }
#else
    /*
     * Ensure that the directory files are correctly replicated:
     */
    for (j = 0; j < NBULLETS; j++) {
	int k;

	if (PORTCMP(&filecaps[j].cap_port, &bulcaps[j].cap_port)) {
	    /* Already is on the right file server;  skip it */
	    continue;
	}

	if (!fsvr_avail(&bulcaps[j].cap_port)) {
	    /* No sense in replicating to a down file server */
	    continue;
	}
	    
	/*
	 * Bullet file is missing or has wrong port.  Create it or move it.
	 * We prefer to use std_copy on one of the good files, if
	 * possible.  Otherwise write the file out from the cache.
	 */
	err = SP_UNREACH;

	for (k = 0; k < NBULLETS; k++) {
	    if (good_file[k]) {
		/* Try to create a new replica using filecaps[k] */
		capability newcap;

		err = std_copy(&bulcaps[j], &filecaps[k], &newcap);

		if (err == STD_OK) {
		    if (NULLPORT(&filecaps[j].cap_port)) {
			MON_EVENT("watchdog: added directory file");
		    } else {
			MON_EVENT("watchdog: replaced directory file");
		    }

		    filecaps[j] = newcap;
		    break;
		} else {
		    MON_EVENT("watchdog: std_copy failed");
		    message("watchdog: dir %ld: std_copy failed (%s)",
			    obj, err_why(err));
		}
	    }
	}

	if (err != STD_OK) {
	    /* Well for some reason it seems we don't have a useable file.
	     * But we can still repair the damage if we have the directory
	     * in the cache.
	     * Note that this is pure guess-work. The destination file server
	     * might be the cause of the problem as well.
	     */
	    if (in_cache(obj)) {
		err = dirf_modify(obj, NBULLETS, filecaps, filecaps, bulcaps);
		if (err == STD_OK) {
		    MON_EVENT("restored file from cached dir");
		    message("watchdog: dir %ld: restored from cache", obj);
		} else {
		    scream("watchdog: cannot replicate cached dir %ld (%s)",
			   obj, err_why(err));
		    /* Don't throw it away;  maybe the disk is sick or full. */
		}
	    } else {
		scream("watchdog: cannot replicate cached-out dir %ld", obj);
		MON_EVENT("cannot replicate cached-out dir");
	    }
	}
    }

    /* If one of the caps was modified, create an intention for this dir.
     * Note that as a result of this intention, the directory may
     * get thrown away in case all its capabilities are null.
     * That in turn only happens in case we had positive evidence
     * that all its current replicas have a bad capability.
     * Until we have a Soap equivalent "fsck", other problems will
     * have to be fixed manually with "sp_fix."
     */
    for (j = 0; j < NBULLETS; j++) {
	if (!cap_cmp(&origcaps[j], &filecaps[j])) {
	    MON_EVENT("watchdog: added intention");
	    intent_add(obj, filecaps);
	    break;
	}
    }
#endif
}

/*
 * The watchdog touches current directory replicas, and creates missing ones.
 * Currently, we do a simple linear search of the super file.
 * Alternatively, we could keep track of unreplicated files in separate lists,
 * one per server.
 *
 * Out of concern for efficiency, we do at most MAX_WATCHDOG directories
 * on every call.  This limits the length of time that SOAP is off the air,
 * and reduces the possibility of a commit failure.
 */
void
watchdog() 
{
    int     ndone;
    objnum  obj;
    static objnum first_entry = 1;
    /* First directory that should have its bullet files touched or created. */

    fsvr_check_svrs();
    if (fsvr_nr_avail() == 0) {
	MON_EVENT("watchdog: no Bullet servers available");
	scream("watchdog: no Bullet servers available");
	sp_end();
	return;
    }
    
    /* Begin at the place we left off on previous call: */

    for (obj = first_entry, ndone = 0;
	 obj < _sp_entries && ndone < MAX_WATCHDOG;
	 obj++)
    {
#ifndef SOAP_GROUP
	if (intent_avail() == 0) {
	    break;
	}
#endif

	if (!sp_in_use(&_sp_table[obj])) {
	    continue;
	}

	do_watchdog(obj);	/* possibly restores object from disk */

	if (fsvr_nr_avail() == 0) {	/* give up */
	    MON_EVENT("watchdog: no bullet servers available");
	    scream("watchdog: no Bullet servers available");
#ifndef SOAP_GROUP
	    intent_clear();
#endif
	    sp_end();
	    return;
	}

	++ndone;
    }
    
    if (sp_commit() != STD_OK) {
	MON_EVENT("watchdog: commit failed");
    }

    /* Incr starting point for next time: */
    first_entry = obj < _sp_entries ? obj : 1;
}
