/*	@(#)lseek.c	1.3	94/04/07 10:28:51 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* lseek() POSIX 6.5.2
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include "fdstuff.h"
#include <sys/types.h>
#include <unistd.h> /* For SEEK_SET etc. */
#include <server/bullet/bullet.h>

off_t
lseek(i, pos, mode)
	int i;
	off_t pos;
	int mode;
{
	struct fd *f;
	
	FDINIT();
	FDLOCK(i, f);

	if (S_ISFIFO(f->fd_mode)) {
		FDUNLOCK(f);
		ERR(ESPIPE, "lseek: seek on pipe");
	}

#ifndef NO_SESSION
	if (f->fd_flags & FUNLINK) { /* seek on shared file descriptor */
		extern errstat ses_seek();
		capability owner;

		if (_ajax_session_owner(&owner) == 0 &&
		    ses_seek(&owner, &f->fd_cap, pos, mode, &pos) == STD_OK) {
		    FDUNLOCK(f);
		    f->fd_pos = pos;
		    return(pos);
		} else {
		    FDUNLOCK(f);
		    ERR(ESPIPE, "session seek failed");
		}
	}
#endif

	if (!(f->fd_flags&FBULLET)) {
		FDUNLOCK(f);
		ERR(ESPIPE, "lseek: not a bullet file");
	}
	switch (mode) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		pos += f->fd_pos;
		break;
	case SEEK_END:
		if (f->fd_size < 0) {
			if (b_size(&f->fd_cap, (b_fsize *) &f->fd_size) != STD_OK) {
				FDUNLOCK(f);
				ERR(EIO, "lseek: b_size failed");
			}
		}
		pos += f->fd_size;
		break;
	default:
		FDUNLOCK(f);
		ERR(EINVAL, "lseek: bad mode");
	}
	if (pos < 0) {
		FDUNLOCK(f);
		ERR(EINVAL, "lseek: bad pos");
	}
	f->fd_pos = pos;
	FDUNLOCK(f);
	return pos;
}
