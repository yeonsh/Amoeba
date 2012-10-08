/*	@(#)reinit.c	1.6	96/02/27 10:11:31 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "monitor.h"
#include "cmdreg.h"
#include "stderr.h"
#include "svr.h"
#include "module/rnd.h"

/*
 *	Start using a new configuration
 */
errstat
NewConf(newconf, cnt)
    boot_data newconf[];
    int cnt;
{
    int old, new;
    Time_t now;

    (void) KillMap();
    MON_EVENT("NewConf");
    if (debugging || verbose) prf("%nNewConf: %d confs\n", cnt);

    /* First try to kill any old configuration that is not in the new config */
    for (old = 0; old < MAX_OBJ; ++old) {
	mu_lock(&objs[old].or_lock);
	if (obj_in_use(&objs[old])) {
	    for (new = 0; new < cnt; ++new) {
		if (!strcmp(objs[old].or.or_data.name, newconf[new].name)) {
		    break;
		}
	    }
	    if (new == cnt) {	/* Not found - kill */
		if (!down(objs + old))
		    prf("%ncouldn't kill obsolete %s - too bad\n",
			objs[old].or.or_data.name);
		objs[old].or.or_data.name[0] = '\0';	/* Invalidate */
	    }
	}
	MU_CHECK(objs[old].or_lock);
	mu_unlock(&objs[old].or_lock);
    }

    now = my_gettime();

    /* Change old configs I know of: */
    for (old = 0; old < MAX_OBJ; ++old) {
	/* TO DO: catch SIG_AMOEBA here? */
	mu_lock(&objs[old].or_lock);
	if (obj_in_use(&objs[old])) {
	    for (new = 0; new < cnt; ++new) {
		if (!strcmp(objs[old].or.or_data.name, newconf[new].name)) {
		    break;
		}
	    }
	    if (new < cnt) {	/* Found */
		objs[old].or.or_data = newconf[new];
		objs[old].or.or_nextpoll = now;
		newconf[new].name[0] = '\0';
	    }
	}
	MU_CHECK(objs[old].or_lock);
	mu_unlock(&objs[old].or_lock);
    }

    /* Insert new configs: */
    for (new = 0; new < cnt; ++new) {
	if (newconf[new].name[0] != '\0') {
	    int ind;
	    port random;

	    ind = new_obj();
	    if (ind < 0) {
		MON_EVENT("Should not happen in NewConf");
		return STD_SYSERR;
	    }
	    objs[ind].or.or_data = newconf[new];
	    objs[ind].or.or_nextpoll = now;/* The timer thread will find this */
	    uniqport(&random);
	    prv_encode(&objs[ind].or.or_objcap.cap_priv,
		    UniqNum(), PRV_ALL_RIGHTS, &random);
	    MU_CHECK(objs[ind].or_lock);
	    mu_unlock(&objs[ind].or_lock);
	}
    }
    MkMap(objs, SIZEOFTABLE(objs));

    return STD_OK;
} /* NewConf */
