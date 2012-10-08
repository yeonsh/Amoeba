/*	@(#)fstat.c	1.3	94/04/07 10:27:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* fstat(2) system call emulation */

#include "ajax.h"
#include "fdstuff.h"

int
fstat(i, buf)
	int i;
	struct stat *buf;
{
	struct fd *f;
	int err;
	
	FDINIT();
	FDLOCK(i, f);

#ifndef NO_SESSION
	if (f->fd_flags & FUNLINK) {
	    /* fstat on shared file descriptor. In that case we must first
	     * ask the session server how big the file is, currently.
	     */
	    extern errstat ses_size();
	    capability owner;
	    long size;

	    if (_ajax_session_owner(&owner) == 0 &&
		ses_size(&owner, &f->fd_cap, &size) == STD_OK) {
		f->fd_size = size;
	    }
	}
#endif

	err = _ajax_capstat(&f->fd_cap, buf, f->fd_mode, f->fd_size);
	if (err == 0) {
	    if (f->fd_mtime == 0) {
		/* no modification time stored yet */
		if ((f->fd_flags & FWRITE) != 0) {
		    /* We are writing it ourselves, so just fill in
		     * the current time.
		     */
		    (void) _ajax_time(&f->fd_mtime);
		} else {
		    /* The file is being read, but when we opened it we didn't
		     * look what the modification time was.  Do it now.
		     */
		    (void) _ajax_lookup(&f->fd_dircap, f->fd_name, NILCAP,
		    		        (int *) NULL, &f->fd_mtime);
		}
	    }
	    buf->st_mtime = f->fd_mtime;
	}
	FDUNLOCK(f);
	return err;
}
