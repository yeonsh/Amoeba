/*	@(#)ajreplace.c	1.5	96/02/27 11:05:23 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Replace capability for a directory entry */

#include "ajax.h"

static errstat
_ajax_replace(dir, path, pcap)
	capability *dir;
	const char *path;
	capability *pcap;
{
	capset csdir, csnew;
	errstat err;
	
	if ((err = _ajax_csorigin(dir, path, &csdir)) != STD_OK)
		return err;
	if (!cs_singleton(&csnew, pcap)) {
		cs_free(&csdir);
		return STD_NOSPACE;
	}
	err = sp_replace(&csdir, path, &csnew);
	cs_free(&csnew);
	cs_free(&csdir);
	return err;
}

errstat
_ajax_install(replace, dircap, name, cap)
int         replace;
capability *dircap;
const char *name;
capability *cap;
/* Install a (possibly new) version of a bullet file under name in dircap.
 * If replace is true, the file existed when it was opened, so to avoid
 * filling up the bullet server, we will try to destroy the current version.
 * This has as side-effect that any link to this file will become invalid.
 */
{
    errstat err;
    interval t;

    /*
     * If you've gotten this far you shouldn't lose your object because the
     * name server is too busy.
     */
    t = timeout((interval) 30000);

    if (!replace) {
        /* First try to append it. When it was opened it did not exist yet. */
        err = _ajax_append(dircap, name, cap);
    }

    if (replace || err == STD_EXISTS) {
        errstat look;
        capability orig;

        look = _ajax_lookup(dircap, name, &orig, (int *)0, (long *)0);

        /* First check if capability has changed.  Even though the file has
         * been written to, this need not be the case (the bullet server
         * has the feature of `compare-creating' an existing file!).
         */
        if (look == STD_OK && memcmp((_VOIDSTAR) &orig,
				(_VOIDSTAR) cap, sizeof(capability)) == 0) {
            err = STD_OK;
        } else {
            /* First install new version. */
            if (look == STD_NOTFOUND) { /* we have to append it after all */
                err = _ajax_append(dircap, name, cap);
            } else {
                err = _ajax_replace(dircap, name, cap);
            }

            /* If all went well we can destroy the old version. */
            if (look == STD_OK && err == STD_OK) {
#ifndef KEEP_ORIGINAL
		/* Destroy the previously installed version.
		 * To do: use cs_purge rather than std_destroy so that all
		 * reachable versions are destroyed. Now we still rely a bit
		 * on garbage collection.
		 */
                if (std_destroy(&orig) != STD_OK) {
                    PUTS("could not destroy original");
                    /* don't care, at least we have tried */
                }
#endif
            } else {
                PUTS("cannot append/replace filename");
            }
        }
    }
    (void) timeout(t);
    return(err);
}
