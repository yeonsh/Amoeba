/*	@(#)opencap.c	1.3	94/04/07 10:30:25 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Open a file given by capability.
   This is almost like open(), except that we can't open bullet files for
   writing since we don't know the parent directory.  For this reason,
   the create mode is not used.
*/

#include "ajax.h"
#include "fdstuff.h"
#include "ampolicy.h"

int
opencap(cap, flags)
	capability *cap;
	int flags;
{
	struct fd *f;
	int i;
	errstat err;
	
	if ((i = _ajax_newfd(&f)) < 0)
		return -1;
	
	switch (flags&(O_RDONLY|O_WRONLY|O_RDWR)) {
	case O_RDONLY:
		f->fd_flags = FREAD;
		break;
	case O_WRONLY:
		f->fd_flags = FWRITE;
		break;
	case O_RDWR:
		f->fd_flags = FREAD | FWRITE;
		break;
	default:
		JUNKNEWFD(i, f);
		ERR(EINVAL, "opencap: invalid r/w flags");
	}
	if (flags&O_APPEND)
		f->fd_flags |= FAPPEND;
	
	f->fd_name[0] = '\0';

	/* ajax_time() is only called when f->fd_mtime is really needed */

	f->fd_cap = *cap;
	/* PM: f->fd_dircap, f->fd_mode, f->fd_mtime */
	/* Is it a bullet file? */
	if ((err = b_size(&f->fd_cap, &f->fd_size)) != STD_OK) {
		/* No bullet file; what else could it be? */
		if (err != STD_COMBAD) {
			/* Any error except a clear reply from the
			   server saying the object may be fine
			   but it doesn't understand b_size is
			   taken as a sign that the capability
			   is invalid: either the server is down
			   or the object has been deleted or the
			   capability has insufficient rights.
			   Servers that don't return STD_COMBAD
			   when they should get what they deserve. */
			JUNKNEWFD(i, f);
			ERR(EIO, "opencap: invalid capability");
		}
		if (_is_am_dir(&f->fd_cap)) {
			JUNKNEWFD(i, f);
			ERR(EISDIR, "opencap: can't open directory");
		}
		/* Alive; assume tty or similar */
		f->fd_size = -1;
		f->fd_pos = -1;
		f->fd_mtime = 0;
		f->fd_mode = 0777; /* Set format to unknown */
	}
	else if (f->fd_flags&FWRITE) {
		/* Can't open bullet files for writing */
		JUNKNEWFD(i, f);
		ERR(EPERM, "opencap: can't open bullet file for writing");
	}
	else {
		/* Open bullet file for reading */
		f->fd_pos = 0;
		f->fd_mode = _S_IFREG | 0777;
		f->fd_mtime = 0;
		f->fd_flags |= FBULLET;
	}
	
	if (i < 3)
		_ajax_setstdcap(i, &f->fd_cap);
	RETNEWFD(i, f);
}
