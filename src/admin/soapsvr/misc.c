/*	@(#)misc.c	1.4	96/02/27 10:22:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/stdcmd.h"
#include "stderr.h"
#include "semaphore.h"
#include "module/mutex.h"
#include "monitor.h"
#include "thread.h"

#include "diag.h"

static capability NULLCAP;

#ifdef SOAP_GROUP

/* Producer/consumer implementing destruction of dir files on background */
#define MAXCAPS	10

static capability destr_caps[MAXCAPS];
static int	  destr_in, destr_out;
static int        destr_inited = 0;
static semaphore  destr_empty, destr_filled;
static mutex      destr_lock;

static void
do_destroy(filecap)
capability *filecap;
{
    errstat err;

    if ((err = std_destroy(filecap)) != STD_OK) {
	scream("cannot destroy directory file (%s)", err_why(err));
    } else {
	MON_EVENT("destroyed dirfile");
    }
}

static void
destroyer()
{
    /* initialise semaphores */
    destr_in = destr_out = 0;
    sema_init(&destr_empty, MAXCAPS);
    sema_init(&destr_filled, 0);
    mu_init(&destr_lock);
    destr_inited = 1;

    for (;;) {
	capability filecap;

	/* fetch cap */
	sema_down(&destr_filled);
	mu_lock(&destr_lock);
	filecap = destr_caps[destr_out];
	destr_out = (destr_out + 1) % MAXCAPS;
	mu_unlock(&destr_lock);
	sema_up(&destr_empty);
	
	/* destroy it */
	do_destroy(&filecap);
	MON_EVENT("destroyed dirfile on background");
    }
}

static int
store_cap(filecap)
capability *filecap;
{
    if (sema_trydown(&destr_empty, (interval) 0) < 0) {
	MON_EVENT("no room to store cap to destroy");
	return 0;
    }

    /* add it to destr_caps */
    mu_lock(&destr_lock);
    destr_caps[destr_in] = *filecap;
    destr_in = (destr_in + 1) % MAXCAPS;
    mu_unlock(&destr_lock);
    sema_up(&destr_filled);
    MON_EVENT("stored cap to destroy");
    return 1;
}

static void
bg_destroy(filecap)
capability *filecap;
{
    /* if we cannot store the capability for later destruction,
     * destroy it right away.
     */
    if (!destr_inited || !store_cap(filecap)) {
	do_destroy(filecap);
    }
}

#endif

void
destroy_file(filecap)
capability *filecap;
/* cleanup a file containing old version of a directory */
{
#ifdef SOAP_GROUP
    /* In case of the Group Soap, the destroy_files happens in the critical
     * path of directory modifications, so we want to do this on background,
     * of possible.
     */
    static int inited = 0;

    if (!inited) {
	inited = 1;
	if (!thread_newthread(destroyer, 5000, (char *) 0, 0)) {
	    scream("cannot start the dirfile destroy thread");
	}
    }
    bg_destroy(filecap);
#else
    /* temporarily do it right away, although it is more efficient to
     * put it in a queue that is handled by a seperate cleanup thread.
     */
    (void) std_destroy(filecap);
#endif
}

void
clear_caps(caplist, ncaps)
capability *caplist;
int ncaps;
{
    int i;

    for (i = 0; i < ncaps; i++) {
	caplist[i] = NULLCAP;
    }
}

int
caps_zero(caplist, ncaps)
capability *caplist;
int ncaps;
{
    int i, zerocaps = 0;

    for (i = 0; i < ncaps; i++) {
	if (NULLPORT(&caplist[i].cap_port)) {
	    zerocaps++;
	}
    }
    return zerocaps;
}
