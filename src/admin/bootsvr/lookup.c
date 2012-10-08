/*	@(#)lookup.c	1.3	94/04/06 11:39:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "stderr.h"
#include "cmdreg.h"
#include "monitor.h"
#include "svr.h"

/*
 *	Lookup an boot_ref.
 */
errstat
ref_lookup(ref, cap, boot)
    boot_ref *ref;
    capability *cap;
    obj_rep *boot;
{
    errstat err;
    char *path;

    for (path = ref->path; *path == '/'; ++path)
	;
    if (path[0] != '\0') {
	if (badport(&ref->cap)) {
	    MON_EVENT("Mimic lookup failure: badport");
	    return RPC_NOTFOUND;
	}
	err = dir_lookup(&ref->cap, path, cap);
	if (err == RPC_FAILURE) 
	    err = dir_lookup(&ref->cap, path, cap);
	if (debugging && err == STD_NOTFOUND) {
	    err = dir_lookup(&ref->cap, path, cap);
	    if (err != STD_NOTFOUND)
		prf("%n\007lookup %s first said 'not found', now %s\n",
						path, err_why(err));
	}

	if (err != STD_OK) {
	    if (boot != NULL && boot->or.or_errstr[0] != '\0')
		sprintf(boot->or.or_errstr, "cannot lookup %s (%s)",
					path, err_why(err));
	    if (debugging) prf("%nLookup '%s' in %s failed (%s)\n",
		path, ar_cap(&ref->cap), err_why(err));
	    (void) capsaid(&ref->cap, err);
	    MON_EVENT("Lookup: cap not found");
	}
	if (err == STD_COMBAD)
	    err = STD_NOTFOUND;
	return err;
    }
    *cap = ref->cap;
    return STD_OK;
} /* ref_lookup */

#if 0
errstat
ref_store(ref, cap)
    boot_ref *ref;
    cap *cap;
{
    if (ref->path[0] != '\0')
	 return dir_replace(dir, path, cap);
    ref->dir = *cap;
    return STD_OK;
}
#endif
