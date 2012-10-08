/*	@(#)ajwatchdog.c	1.3	96/02/27 11:05:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ajax.h"
#include "fdstuff.h"
#include "bullet/bullet.h"
#include "thread.h"

#if BS_CACHE_TIMEOUT < 4
 #error I do not believe this: uncommitted bullet files time out much too fast!
#else
/* we have to be a bit faster (say 3 minutes) than the bullet timeout: */
#define TIMEOUT_SECS	(60*(BS_CACHE_TIMEOUT - 3))
#endif

/* the file descriptors we are interested in have the following flags: */
#define BULLET_FLAGS	(FBULLET|FWRITE|FNONCOMMITTED)

extern unsigned int sleep();

/*
 * Regularly touch uncommitted bullet files to avoid losing them.
 */
static void
fd_watchdog(p, s)
char *	p;
int	s;
{
    FDINIT();
    for (;;) {
	register int i;

	/* Do what watchdogs do most of the time: */
	(void) sleep(TIMEOUT_SECS);

	/* Time for some action: touch uncommitted bullet files */
	MU_LOCK(&_ajax_fdmu); /* avoid fd fiddling while we're busy*/
	for (i = 0; i < NOFILE; i++) {
	    register struct fd *f = FD(i);

	    /* first look if this one is in use: */
	    if (f == NILFD || f == NOFD) {
		continue;
	    }
	    f = CLEANFD(f);
	    if (f->fd_refcnt > 0) {
		/* look if it's a fd to be checked before actually trying to
		 * lock it, otherwise we might get blocked on a fd we are
		 * not interested in (e.g. a tty fd locked in read()).
		 */
		if ((f->fd_flags & BULLET_FLAGS) == BULLET_FLAGS) {
		    char buf[1];

		    MU_LOCK(&f->fd_mu);
		    /*
		     * touch the uncomitted bullet file by writing 0 bytes
		     * at offset 0.  Ignore return value, because we cannot
		     * do anything about it.
		     */
		    (void) b_modify(&f->fd_cap, 0L, buf, 0L, 0, &f->fd_cap);
		    MU_UNLOCK(&f->fd_mu);
		}
	    }
	}
        MU_UNLOCK(&_ajax_fdmu);
    }
}

void
_ajax_fd_watchdog()
/* starts the file descriptor watchdog in a new thread */
{
    static int started = 0;

    if (!started) {
	started = 1;
	(void) thread_newthread(fd_watchdog, 2000, (char *)NULL, 0);
    }
}
