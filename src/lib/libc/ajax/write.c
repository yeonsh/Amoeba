/*	@(#)write.c	1.7	96/02/27 11:06:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* write() POSIX 6.4.2 
 * Modified:
 *	apr 22 93  Ron Visser: Posix fixes
 *	july 6 93  Kees Verstoep: merged in Ron's fixes & cleaned up.
 */

#include "ajax.h"
#include "fdstuff.h"
#include <unistd.h>

static int
do_write(f, buf, n)
struct fd *f;
_VOIDSTAR  buf;
size_t     n;
{
    errstat err;
    int nwritten;

    if (f->fd_flags & FBULLET) {
	err = b_modify(&f->fd_cap, f->fd_pos, (char *) buf,
		       (b_fsize) n, 0, &f->fd_cap);
	switch (err) {
	case STD_OK:
	    f->fd_flags |= FNONCOMMITTED;
	    nwritten = n;
	    break;
	case STD_NOSPACE:
	    ERR(ENOSPC, "write: no space");
	    break;
	case STD_NOMEM:
	    ERR(ENOMEM, "write: no memory from b_modify");
	    break;
	default:
	    ERR(EINVAL, "write: b_modify failed");
	    break;
	}
    } else {
	nwritten = fswrite(&f->fd_cap, f->fd_pos, (char *) buf, (long) n);
	if (nwritten == STD_NOTNOW) {
              /* raise(SIGPIPE); */ /* currently adds 10K to the binary :-( */
              ERR(EPIPE, "write: no readers on pipe");
	} else if (nwritten != n) {
              ERR(_ajax_error(nwritten), "write: fswrite failed");
        }
    }

    f->fd_pos += nwritten;
    if (f->fd_size >= 0 && f->fd_pos > f->fd_size) {
	f->fd_size = f->fd_pos;
    }
    return nwritten;
}

/* Append a zero-filled space of size 'n' to the file.
 * This is needed because the Bullet server doesn't currently support
 * discontiguous files.  No attempt is made to do this efficiently.
 */
static int
writezeros(f, n)
struct fd *f;
size_t     n;
{
    char buf[1024];
 
    (void) memset((_VOIDSTAR) buf, 0, sizeof buf);
    while (n > 0) {
	int len;

	len = (n > sizeof buf) ? sizeof buf : n;
        if (do_write(f, buf, len) != len) {
	    return -1;
	}
	n -= len;
    }
    return 0;
}

ssize_t
write(i, buf, n)
int i;
_VOIDSTAR buf;
size_t n;
{
    struct fd *f;
    long nwritten;
    interval t;
	
    FDINIT();
    FDLOCK(i, f);
    if ((f->fd_flags & FWRITE) == 0) {
	FDUNLOCK(f);
	ERR(EBADF, "write: not open for writing");
    }

    /* Make sure that writes have a good chance of succeeding */
    t = timeout((interval) 30000);

    /* If we write beyond the EOF of a bullet file we need to insert zeros! */
    if ((f->fd_flags & FBULLET) != 0 && (f->fd_size >= 0) &&
        (f->fd_pos > f->fd_size))
    {
	long curpos;

	/* first seek back to the end of the file */
	curpos = f->fd_pos;
	f->fd_pos = f->fd_size;
	if (writezeros(f, curpos - f->fd_size) < 0) {
	    FDUNLOCK(f);
	    (void) timeout(t);
	    return -1;
	}
	f->fd_pos = curpos;
    }

    nwritten = do_write(f, buf, n);
    FDUNLOCK(f);
    (void) timeout(t);
    return nwritten;
}
