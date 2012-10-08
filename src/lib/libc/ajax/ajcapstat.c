/*	@(#)ajcapstat.c	1.4	94/04/07 10:22:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Return approximated stat(2) info from a capability */

#include "ajax.h"
#include "stdobjtypes.h"

#include <sys/types.h>
#include <sys/stat.h>

#define PORT2SHORT(p) (_FP(p)->_p2)
#define PORT2LONG(p) (_FP(p)->_p1)

int
_ajax_capstat(cap, buf, mode, size)
	capability *cap;
	struct stat *buf;
	int mode;
	long size;
{
	long savetimeout;
	errstat err;
	static struct stat szero; /* Constant initialized to all zeros */
	
	*buf = szero; /* Clear all unused fields in one fell swoop */
	
	/* Fill in st_dev and st_ino with bits from the capability.
	   This is needed since some programs check for file identity
	   that way (e.g., cat A >B checks that A isn't B).  Assume that
	   servers assign different object numbers to different objects
	   (and don't just distinguish them by random number). */
	buf->st_dev = PORT2SHORT(&cap->cap_port);
	buf->st_ino = prv_number(&cap->cap_priv);
	buf->st_nlink = 1;
	buf->st_uid = FAKE_UID;
	buf->st_gid = FAKE_GID;
	
	/* Set a short timeout so we don't wait too long for dead
	   servers; save the value to restore it later. */
	savetimeout = timeout(2000L);
	
	/* Check for directories first since kernels pretend to be
	   both bullet files and directories, but we definitely want
	   them to look like directories here.  Unfortunately this
	   costs an extra RPC per stat of a bullet file, sigh. */
	if ((mode&_S_IFMT) == 0) {
		if (_is_am_dir(cap))
			mode |= _S_IFDIR;
	}
	
	/* Get the size if unknown and it may be a regular file */
	if (size < 0 && ((mode&_S_IFMT) == 0 || (mode&_S_IFMT) == _S_IFREG)) {
		if ((err = b_size(cap, &size)) != STD_OK) {
			if (err != STD_COMBAD) {
				/* Any error except a clear reply from
				   the server saying the object may be
				   fine but it doesn't understand
				   b_size() is taken as a sign that the
				   capability is invalid: either the
				   server is down or the object has been
				   deleted or the capability has
				   insufficient rights.  Servers that
				   don't return STD_COMBAD when they
				   should get what they deserve. */
				timeout(savetimeout);
				ERR(ENOENT,
					"_ajax_capstat: invalid capability");
			} else {
				/* see if it's a fifo */
				char info[4];
				int siz;

				err = std_info(cap, info, sizeof(info), &siz);
				if (err == STD_OK || err == STD_OVERFLOW) {
					if (info[0] == OBJSYM_PIPE[0]) {
						mode |= _S_IFIFO;
					}
				}
			}
			size = -1;
		}
		else
			mode |= _S_IFREG;
	}
	
	timeout(savetimeout);
	
	/* If the mode if still unknown, make it a character special file */
	if ((mode&_S_IFMT) == 0) {
		mode |= _S_IFCHR;
		/* As a courtesy to ls(1) users, set st_rdev using part
		of the port as major, the lower bits of the object
		number as minor device no.  Use the values already set
		earlier in st_dev and st_ino. */
		buf->st_rdev =
			(buf->st_dev & 0xff00) | (buf->st_ino & 0xff);
	}
	
	buf->st_mode = mode;
	if (size >= 0) {
		buf->st_size = size;
		buf->st_blksize = BUFFERSIZE;
#ifdef DEV_BSIZE
		buf->st_blocks = (size + DEV_BSIZE - 1) / DEV_BSIZE;
#endif
	}
	return 0;
}
