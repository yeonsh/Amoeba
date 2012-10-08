/*	@(#)close.c	1.7	96/02/29 16:46:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* close(2) system call emulation */

#include "ajax.h"
#include "fdstuff.h"
#include "sesstuff.h"
#include <limits.h>
#include "fifo/fifo.h"

int
close(i)
	int i;
{
	FDINIT();
	return _ajax_close(i, NILFD, 1);
}

/* Code shared between close, dup2, execlose and fdinit.closefiles */

static int unshare_pipe();

int
_ajax_close(i, newf, lock)
	int i;
	struct fd *newf;
	int lock;	/* true if we must wait for lock to do close */
{
	struct fd *f;
	int err = 0;
	
	if (lock)
	{
		FDLOCK3(i, f, (void *)0); /* Keep _ajax_fdmu locked */
	}
	else
	{
		/* If preemptive scheduling is introduced then we must
		 * switch it off while we do the close!
		 *
		    DISABLE_PREEMPTIVE_SCHEDULING;
		 */
		if ((f = FD(i)) == NILFD || f == NOFD)
			ERR(EBADF, "close: invalid handle");
		f = CLEANFD(f);
	}
	if (i < 3)
		_ajax_setstdcap(i, NILCAP);
	_ajax_fdmap[i] = newf;
	if (lock)
		MU_UNLOCK(&_ajax_fdmu);
	if (f->fd_refcnt == 1) {
		/* Really close it: */
		if ((f->fd_flags & FWRITE) && (f->fd_flags & FBULLET))
			err = _ajax_commitfd(f);
		else if (f->fd_flags & FFIFO)
			err = fifo_close(&f->fd_cap);
		else if (f->fd_flags & FUNLINK)
			err = unshare_pipe(f);
		JUNKFD(f);
	}
	else
		f->fd_refcnt--;
	if (lock)
	{
		FDUNLOCK(f);
	}
	return err;
}

int
_ajax_commitfd(f)
	struct fd *f;
{
	if (f->fd_flags & FNONCOMMITTED) {
		if (b_modify(&f->fd_cap, 0L, (char *)0, 0L, BS_COMMIT,
							&f->fd_cap) != STD_OK)
			ERR(EIO, "_ajax_commitfd: can't commit");
		f->fd_flags &= ~(FNONCOMMITTED|FSTORED);
	}
	
	if (f->fd_flags & FSTORED)
		return 0;
	
	if (_ajax_install((f->fd_flags & FREPLACE) != 0,
			 &f->fd_dircap, f->fd_name, &f->fd_cap) != STD_OK)
		ERR(EIO, "_ajax_commitfd: can't replace");

	f->fd_flags |= FSTORED;
	return 0;
}

/* Unshare a pipe.
   Only do this if it appears we are running under the session server.
   (Really, this shouldn't happen if we aren't running under the session
   server, but you can't be too careful.) */

static int
unshare_pipe(f)
	struct fd *f;
{
#ifndef NO_SESSION
	/* Don't do this if you always run without session server.
	   In that case, also remove ses_unshare.c and ajgetowner.c. */
	capability owner;
	
	if (_ajax_session_owner(&owner) < 0)
		ERR(EIO, "unshare_pipe: no session manager");
	if (ses_unshare(&owner, &f->fd_cap, 1) != STD_OK)
		ERR(EIO, "unshare_pipe: ses_unshare failed");
#endif
	return 0;
}
