/*	@(#)intr.c	1.4	96/02/27 10:11:13 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "stdlib.h"
#include "cmdreg.h"
#include "stderr.h"
#include "exception.h"
#include "fault.h"
#include "module/signals.h"
#include "monitor.h"
#include "thread.h"


/*
 *	Module to catch client-to-server (c2s) signal.
 *	Exported (and documented) interface:
 */
extern errstat intr_init();	/* Initialise and reset flag */
extern void intr_reset();	/* reset flag */
extern int intr_c2s();		/* Interrupted by c2s? */

static int thread_cookie;	/* Magic cookie for thread_*() */

/*
 *	This routine gets called on a client-to-server signal.
 *	It raises an exception iff the architecture detects a
 *	NULL-dereference. As of this writing, this is true
 *	for both 68000 and VAX implementations of Amoeba.
 */
/*ARGSUSED*/
static void
intr_catcher(sig, us, extra)
    signum sig;
    thread_ustate *us;
    _VOIDSTAR extra;
{
    MON_EVENT("Catch a client-to-server signal");
    *thread_alloc(&thread_cookie, sizeof(thread_cookie)) = 1;
} /* intr_catcher */

/*
 *	Initialises; returns success as STD_OK, otherwise STD_NOMEM
 */
errstat
intr_init()
{
    register int *p;
    errstat err;
    err = sig_catch((signum) SIG_TRANS, intr_catcher, (_VOIDSTAR) NULL);
    if (err != STD_OK) {
	MON_EVENT("sig_catch failed");
	return err;
    }
    p = (int *) thread_alloc(&thread_cookie, sizeof(thread_cookie));
    if (p == NULL) return STD_NOMEM;
    *p = 0;
    return STD_OK;
} /* intr_init */

void
intr_reset()
{
    *(int *)thread_alloc(&thread_cookie, sizeof(thread_cookie)) = 0;
    MON_EVENT("reset c2s-flag");
} /* intr_reset */

/*
 *	Are we interrupted by c2s?
 */
int
intr_c2s()
{
    return *(int *) thread_alloc(&thread_cookie, sizeof(thread_cookie));
} /* intr_c2s */
