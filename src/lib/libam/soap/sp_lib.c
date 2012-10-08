/*	@(#)sp_lib.c	1.6	96/02/27 11:04:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "stdio.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "tod/tod.h"
#include "monitor.h"
#include "caplist.h"
#include "module/mutex.h"
#include "module/tod.h"
#include "module/direct.h"
#include "module/rnd.h"
#include "module/cap.h"

#include "stdlib.h"
#include "ctype.h"

#define SP_ENTRIES	10
#define NCAPCACHE	10

sp_tab	   *_sp_table;

int 	   _sp_entries;
capability _sp_generic;
capset 	   _sp_rootdir;
capset	   _sp_homedir;
capset	   _sp_workdir;
long 	   _sp_now;

static mutex sp_mutex;

#ifdef OWNDIR
port _sp_getport;	/* get port for Soap requests */
#endif

#ifdef OWNDIR
long
sp_time()
{
	/* time used adding capabilities to in-core directory */
	return _sp_now;
}
#endif

char *
sp_alloc(size, n)
	unsigned size, n;
{
	return (char *) calloc(n, size);
}

void
sp_begin()
{
	mu_lock(&sp_mutex);
}

void
sp_end()
{
	mu_unlock(&sp_mutex);
}

#ifdef OWNDIR

extern sp_tab *sp_first_update(), *sp_next_update();
extern void sp_free_mods(), sp_clear_updates();
extern errstat sp_create_rootdir();

/* Abort any outstanding operations by restoring the directories.  The
 * information necessary to do this has been saved by sp_save_and_mod_dir().
 */
void
sp_abort()
{
    sp_tab *st, *next_st;

    MON_EVENT("abort");
    for (st = sp_first_update(); st != NULL; st = next_st) {
        next_st = sp_next_update();
        (void) sp_undo_mod_dir(st);
        sp_free_mods(st);
    }

    sp_clear_updates();
    sp_end();
}

/* This function is supposed to update the "time-to-live" attribute of the
 * specified entry in the sp_table array.  Servers not implementing the
 * touching-aging method of garbage collection can use this version.  The
 * SOAP server program has its own version:
 */
/*ARGSUSED*/
errstat
sp_refresh(obj, cmd)
objnum obj;	/* An index into sp_table[], specifying object */
command cmd;	/* The command that is about to be performed on obj */
{
    if (obj < 1 || obj >= _sp_entries) {
	return STD_ARGBAD;
    } else {
	return STD_OK;
    }
}

errstat
sp_commit()
{
    sp_tab *st, *next_st;

    MON_EVENT("commit");
    for (st = sp_first_update(); st != NULL; st = next_st) {
	next_st = sp_next_update();
	sp_free_mods(st);
    }

    sp_clear_updates();
    sp_end();
    return STD_OK;
}

#endif

#define Failure(err)	{ retval = err; goto Exit; }

#ifndef OWNDIR

/*
 * default version of sp_init
 */

errstat
sp_init()
{
    static int sp_initialized;
    static errstat retval;	/* cached result */

    if (sp_initialized) {
	/* An optimization to avoid needless mutex ops */
	return retval;
    }

    sp_begin();
    if (sp_initialized) {
	/* multiple threads called sp_init concurrently */
	sp_end();
	return retval;
    }

#ifdef AMOEBA
    {
	capability *cap;

	if ((cap = getcap("ROOT")) != NULL) {
	    if (cs_singleton(&_sp_rootdir, cap) == 0) {
		Failure(STD_NOMEM);
	    }
	}

	if ((cap = getcap("WORK")) != NULL) {
	    if (cs_singleton(&_sp_workdir, cap) == 0) {
		Failure(STD_NOMEM);
	    }
	} else {
	    if (cs_copy(&_sp_workdir, &_sp_rootdir) == 0) {
		Failure(STD_NOMEM);
	    }
	}
    }
#else
    {
	/* Under Unix, we have to look in the .capability file */
	capability root_cap;

	if (dir_root(&root_cap) != 0) {
	    Failure(STD_CAPBAD);
	}
	if (cs_singleton(&_sp_homedir, &root_cap) == 0) {
	    Failure(STD_NOMEM);
	}
    
	if (cs_copy(&_sp_rootdir, &_sp_homedir) == 0) {
	    Failure(STD_NOMEM);
	}
	if (cs_copy(&_sp_workdir, &_sp_homedir) == 0) {
	    Failure(STD_NOMEM);
	}
    }
#endif

    /* success */
    retval = STD_OK;

Exit:
    sp_initialized = 1;
    sp_end();

    return retval;
}

#else

/*
 * OWNDIR version of sp_init
 */

#define CAP_NAME_MAX	64
  
static int init_thread_nr = -1;

#ifdef AMOEBA
#include "thread.h"
#endif

static errstat
my_thread_nr(thread_nr)
int *thread_nr;
{
    int thread_id;
    
#ifdef AMOEBA
    static int   ident;
    int 	*p_count;

    if ((p_count = (int *)thread_alloc(&ident, sizeof(int))) == NULL) {
        return STD_NOSPACE;
    }

    if (*p_count == 0) {	/* first time; invent and store id */
	static int   count;
	static mutex count_mu;

	mu_lock(&count_mu);
	thread_id = ++count;	/* number threads from 1 upwards */
	mu_unlock(&count_mu);

	*p_count = thread_id;
    } else {			/* retrieve stored id for this thread */
	thread_id = *p_count;
    }
#else
    /* simple: single threaded */
    thread_id = 1;
#endif

    /* success */
    *thread_nr = thread_id;
    return STD_OK;
}

errstat
sp_init()
{
    static char   *def_colnames[] = { "owner", "group", "other", 0 };
    static long    cols[] = { 0xFF, 0, 0 };
    static int     sp_initialized;
    static mutex   init_mu;
    static errstat retval;	/* cached result */
    int            thread_nr;
    capability    *rootcap;
    port           root_check;

    int	    millisecs, tz, dst;
    errstat err;
    capset  cs;		/* Note: deallocating cs on error is not worth the
			 * trouble (it can only happen once anyway).
			 */

    if (sp_initialized) {
	/* An optimization to avoid needless mutex ops */
	return retval;
    }

    /* Careful now!  As this sp_init() uses the Soap stubs, it will itself
     * be called recursively.  So we would simply try to grab the lock here,
     * we would lock ourselves up.  To avoid this, we let the initialising
     * thread pass, the second time it comes here.
     */
    err = my_thread_nr(&thread_nr);
    if (err != STD_OK) {
	retval = err;
	return retval;
    }

    if (thread_nr == init_thread_nr) {	/* avoid deadlock mentioned */
	return STD_OK;
    }

    mu_lock(&init_mu);
    if (sp_initialized) {
	/* multiple threads called sp_init concurrently */
	mu_unlock(&init_mu);
	return retval;
    } else {
	/* mark the thread doing the initialisation */
	init_thread_nr = thread_nr;
    }

    _sp_entries = SP_ENTRIES;
    _sp_table = (sp_tab *) calloc(SP_ENTRIES, sizeof(*_sp_table));
    if (_sp_table == NULL) {
	Failure(STD_NOSPACE);
    }

    cc_init(NCAPCACHE);

    /* Temporarily create _sp_rootdir out of ROOT from the cap environ.
     * It is needed to look up the random svr (for uniqport) and the tod svr.
     */
    if ((rootcap = dir_origin("/")) == NULL) {
	/* no ROOT in cap env? */
	Failure(STD_SYSERR);
    }
    if (!cs_singleton(&_sp_rootdir, rootcap)) {
	Failure(STD_NOSPACE);
    }
    uniqport(&_sp_getport);
    priv2pub(&_sp_getport, &_sp_generic.cap_port);

    if (!cs_singleton(&cs, &_sp_generic)) {
	Failure(STD_NOSPACE);
    }

    if ((err = tod_gettime(&_sp_now, &millisecs, &tz, &dst)) != STD_OK) {
	fprintf(stderr, "sp_init: cannot get time (%s)\n", err_why(err));
	/* but no reason to fail just because of that */
    }

    uniqport(&root_check);
    err = sp_create_rootdir(def_colnames, 3, &root_check, &_sp_rootdir);
    if (err != STD_OK) {
	Failure(err);
    }

#ifdef AMOEBA
    {
	extern struct caplist *capv;
	register struct caplist *cl;

	for (cl = capv; cl->cl_name != NULL; cl++) {
	    char name[CAP_NAME_MAX+1];
	    int  i;
	    
	    /* create lower case version of cl_name in name */
	    for (i = 0; i < CAP_NAME_MAX && cl->cl_name[i] != '\0'; i++) {
		int c = cl->cl_name[i];

		name[i] = isupper(c) ? tolower(c) : c;
	    }
	    name[i] = '\0';

	    if (cs_singleton(&cs, cl->cl_cap) == 0) {
		Failure(STD_NOSPACE);
	    }
	    if ((err = sp_append(&_sp_rootdir, name, &cs, 3, cols)) != STD_OK) {
		Failure(err);
	    }
	}
    }
#else
    {
	/* Under Unix, we have to look in the .capability file */
	capability root_cap;

	if (dir_root(&root_cap) != 0) {
	    Failure(STD_CAPBAD);
	}
	cs_singleton(&_sp_homedir, &root_cap);
    
	err = sp_append(&_sp_rootdir, "home", &_sp_homedir, 3, cols);
	if (err != STD_OK) {
	    Failure(err);
	}

	err = sp_append(&_sp_rootdir, "work", &_sp_homedir, 3, cols);
	if (err != STD_OK) {
	    Failure(err);
	}
    }
#endif

    /* Success! */
    retval = STD_OK;

Exit:
    if (retval != STD_OK) {
	if (_sp_table != NULL) {
	    free((_VOIDSTAR) _sp_table);
	    _sp_table = NULL;
	}
    }

    sp_initialized = 1;
    mu_unlock(&init_mu);

    return retval;
}

#endif /* OWNDIR version */
