/*	@(#)ajcommit.c	1.4	94/04/07 09:41:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ajax.h"
#include "fdstuff.h"
#include "sesstuff.h"
#include "fifo/fifo.h"

/* Subroutine for exec to handle bullet files.
   All files open for writing are committed.
   This is mostly a safety measure, but essential for close-on-exec files.
   Assumes no locking is necessary. */

int
_ajax_commitall()
{
	int i;
	int ret = 0;
	
	FDINIT();
	
	for (i = 0; i < NOFILE; ++i) {
		struct fd *f = FD(i);
		if (f == NILFD)
			continue;
		f = CLEANFD(f);
		if (f->fd_flags & FBULLET) {
			if (!(f->fd_flags & FWRITE))
				continue;
			if (_ajax_commitfd(f) < 0)
				ret = -1;
		}
	}
	return ret;
}

/* XXX the following really belongs in a separate file... */

/* Subroutine for fork to handle bullet files.
   All file state for open bullet files is passed on to the session server.
   Assumes no locking is necessary. */

static int share_file();

int
_ajax_shareall()
{
	int i;
	int ret = 0;
	
	FDINIT();
	
	for (i = 0; i < NOFILE; ++i) {
		struct fd *f;

		f = FD(i);
		if (f == NILFD)
			continue;
		f = CLEANFD(f);
		if (f->fd_flags & FBULLET) {
			if (share_file(f) < 0)
				ret = -1;
		} else if (f->fd_flags & FFIFO) {
			if (fifo_share(&f->fd_cap) != STD_OK)
				ret = -1;
		}
	}
	return ret;
}

/* Subroutine for newproc to handle bullet files.
   All file state for open bullet files that will be shared
   is passed on to the session server.
   Assumes no locking is necessary. */

int
_ajax_share(nfd, fdlist)
	int nfd;
	int *fdlist; /* Really fdlist[nfd] */
{
	int i;
	int ret = 0;
	
	FDINIT();
	
	for (i = 0; i < nfd; ++i) {
		struct fd *f;
		int j;
		
		j = fdlist[i];
		if (j < 0 || j >= NOFILE)
			continue;
		f = FD(j);
		if (f == NILFD || GETCLEXEC(f))
			continue;
		f = CLEANFD(f);
		if (f->fd_flags & FBULLET) {
			if (share_file(f) < 0)
				ret = -1;
		} else if (f->fd_flags & FFIFO) {
			if (fifo_share(&f->fd_cap) != STD_OK)
				ret = -1;
		}
	}
	return ret;
}

static int
share_file(f)
	struct fd *f;
{
	capability owner;

	if (_ajax_session_owner(&owner) < 0)
		ERR(EIO, "share_file: can't get owner");
	if (ses_file(&owner, f->fd_flags, f->fd_pos, f->fd_size, f->fd_mode,
		f->fd_mtime, &f->fd_cap, &f->fd_dircap, f->fd_name, &f->fd_cap)
								!= STD_OK)
		ERR(EIO, "share_file: ses_file failed");
	f->fd_flags = (f->fd_flags & (FREAD|FWRITE)) | FUNLINK;
	f->fd_size = -1;
	f->fd_pos = -1;
	f->fd_mode &= 0777; /* Pretend it's a pipe */
	return 0;
}
