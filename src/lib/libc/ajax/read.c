/*	@(#)read.c	1.5	96/02/27 11:05:57 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* read() POSIX 6.4.1 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "fdstuff.h"

ssize_t
read(i, buf, n)
	int i;
	_VOIDSTAR buf;
	size_t n;
{
	struct fd *f;
	b_fsize nread;
	errstat err;
	
	FDINIT();
	FDLOCK(i, f);
	if ((f->fd_flags & FREAD) == 0) {
		FDUNLOCK(f);
		ERR(EBADF, "read: not open for reading");
	}
	if (f->fd_flags & FBULLET) {
		if (f->fd_pos >= f->fd_size && f->fd_size >= 0) {
			FDUNLOCK(f);
			/* Read on bullet file beyond EOF should return 0 */
			return 0;
		}
		if (f->fd_flags & FNONCOMMITTED) {
			if (b_modify(&f->fd_cap, 0L, (char *)0, 0L,
					BS_COMMIT, &f->fd_cap) != STD_OK) {
				FDUNLOCK(f);
				ERR(EIO, "read: commit failed");
			}
			f->fd_flags &= ~(FNONCOMMITTED|FSTORED);
		}
		err = b_read(&f->fd_cap, f->fd_pos, (char *) buf,
			     (b_fsize) n, &nread);
	}
	else {
		nread = fsread(&f->fd_cap, f->fd_pos, buf, (long)n);
		err = (nread >= 0) ? STD_OK : (errstat) nread;
	}
	if (err != STD_OK) {
		FDUNLOCK(f);
		if (err == STD_INTR) {
			/* The trans was interrupted. */
#ifdef NOUSIG
			/* A kernel bug used to forget to call the
			   handler when the interrupted trans returns,
			   so we call it here. */
			sig_raise(2); /* 2 == SIGINT */
#endif
			ERR(EINTR, "read: EINTR");
		}
		else {
			ERR(_ajax_error(err), "read: b_read or fsread failed");
		}
	}
	f->fd_pos += nread;

	FDUNLOCK(f);
	return nread;
}
