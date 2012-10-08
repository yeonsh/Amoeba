/*	@(#)open.c	1.6	96/02/27 11:05:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix open() function */

/* This also implements creat(), through the O_CREAT and O_TRUNC options.
   The various paths through the code are:
   
   - to open for reading only (O_RDONLY; the file must now exist):
	- if b_size succeeds:
		- set FBULLET flag.
   
   - to open for writing and possibly for reading (O_WRONLY or O_RDWR):
	- if new file (and O_CREAT set):
		- set FBULLET flag;
		- create through b_create to DEF_BULLETSVR.
	- if existing file (and O_EXCL not set):
		- - don't even THINK about opening a directory.
		- if b_size succeeds:
			- set FBULLET flag;
			- if need to truncate (O_TRUNC set):
				- send b_create to file's capability
			- if no need to truncate (O_TRUNC not set):
				- send b_modify to file's capability
		- if b_size fails:
			- do nothing; assume it's a tty or device.

*/

#include "ajax.h"
#include "fdstuff.h"
#include "ampolicy.h"
#include "fifo/fifo.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/* The flags that are true if a bullet file needs a watchdog */
#define	BFNW	(FBULLET|FWRITE|FNONCOMMITTED)

static int
installable(f)
struct fd *f;
{
	/* Return the fact whether we can append/replace the directory
	 * entry for bullet file f.
	 * Just hope it stays that way until the close() happens.
	 */
	register long rights;

	rights = f->fd_dircap.cap_priv.prv_rights & PRV_ALL_RIGHTS;

	/* We must have access to at least one of the columns
	 * and we need SP_MODRGT on the directory.
	 */
	return ((rights & SP_COLMASK) != 0 &&
		(rights & SP_MODRGT) == SP_MODRGT);
}
	
static int bulcap_valid;

static errstat
get_bullet_cap(svrcap)
capability *svrcap;
{
    static capability bulcap;
    errstat err;

    if (!bulcap_valid) {
	if ((err = name_lookup(DEF_BULLETSVR, &bulcap)) != STD_OK) {
	    return err;
	}
	bulcap_valid = 1;
    }
    *svrcap = bulcap;
    return STD_OK;
}

#ifdef __STDC__
int open(const char *file, int flags, ...)
#else
/*VARARGS2*/
int
open(file, flags, va_alist)
	char *file;
	int flags;
	va_dcl
#endif
{
	struct fd *f;
	int i;
	errstat err;
	
#ifndef NO_DEV_TTY_SUPPORT
	/* Hack: recognize /dev/tty and substitute getcap("TTY") */
	if (strcmp(file, "/dev/tty") == 0 && getcap("TTY") != NULL)
		return opencap(getcap("TTY"), flags);
	/* NB: if we allow this, we could implement /dev/null similarly
	   as well... */
#endif
	
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
		ERR(EINVAL, "open: invalid r/w flags");
	}
	if (flags&O_APPEND)
		f->fd_flags |= FAPPEND;
	
	/* In case we are only reading the file, we want to avoid extra RPC
	 * just to get the parent directory and file status information.
	 */
	if ((f->fd_flags & FWRITE) == 0) {
		/* In this case we store the origin directory
		 * and the full name given.
		 */
		capability *origin;

		if ((origin = dir_origin(file)) == NULL) {
                        JUNKNEWFD(i, f);
                        ERR(EIO, "open: cannot get origin");
                }

		f->fd_dircap = *origin;
		strncpy(f->fd_name, file, PATH_MAX);
		f->fd_name[PATH_MAX] = '\0';
	} else {
		/* When the file is opened for writing, we need to
		 * get the parent directory first, in order to see if
		 * we have enough rights to store the file when it is closed.
		 */
		file = _ajax_breakpath(NILCAP, file, &f->fd_dircap);
		if (file == NULL) {
			/* This error can mean either be ENOENT (a component of
			 * the path does not exist) or ENOTDIR (a component of
			 * the path prefix is not a directory).  We choose
			 * ENOENT because that's the most common cause.
			 */
			JUNKNEWFD(i, f);
			ERR(ENOENT, "open: bad path");
		}
		strncpy(f->fd_name, file, NAME_MAX);
		f->fd_name[NAME_MAX] = '\0';
	}

	/* Lookup the name without fetching the modification time, thus
	 * saving an RPC.  We can always fetch the modification time later on,
	 * in the (rare) case that an fstat() is done on this descriptor.
	 */
	f->fd_mtime = 0;
	if (_ajax_lookup(&f->fd_dircap, file, &f->fd_cap,
			&f->fd_mode, (long *) NULL) == STD_OK)
	{
		/* File exists */
		if (flags&O_EXCL) {
			JUNKNEWFD(i, f);
			ERR(EEXIST, "open: file exists");
		}
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
				ERR(EIO, "open: invalid capability");
			}
			err = fifo_open(&f->fd_cap, f->fd_flags, &f->fd_cap);
			if (err == STD_OK) {
				f->fd_flags |= FFIFO;
				f->fd_size = -1;
				f->fd_pos = 0;
			} else if (err == STD_COMBAD) {
				if (_is_am_dir(&f->fd_cap)) {
					JUNKNEWFD(i, f);
					ERR(EISDIR, "open: can't open dir");
				}
				/* Alive; assume tty or similar */
				f->fd_size = -1;
				f->fd_pos = -1;
				f->fd_mode &= ~_S_IFMT; /*Set mode to unknown*/
			} else {
				JUNKNEWFD(i, f);
				ERR(_ajax_error(err), "open: can't open fifo");
			}
		}
		else if (f->fd_flags&FWRITE) {
			/* A bullet file, for writing */

			/* check that we can install it later on */
			if (!installable(f)) {
				JUNKNEWFD(i, f);
				ERR(EACCES, "open: cannot modify directory");
			}

			if (flags&O_TRUNC) {
				if (b_create(&f->fd_cap, (char *)NULL, 0,
						0, &f->fd_cap) != STD_OK) {
					JUNKNEWFD(i, f);
					ERR(EPERM, "open: b_create failed");
				}
				f->fd_size = 0;
			}
			else {
				/* Modify an existing one */
				if (b_modify(&f->fd_cap, 0L, (char *)NULL, 0,
						0, &f->fd_cap) != STD_OK) {
					JUNKNEWFD(i, f);
					ERR(EPERM, "open: b_modify failed");
				}
			}
			if (flags&O_APPEND)
				f->fd_pos = f->fd_size;
			else
				f->fd_pos = 0;
			f->fd_mode = _S_IFREG | (f->fd_mode & ~_S_IFMT);

			f->fd_flags |= FBULLET | FNONCOMMITTED | FREPLACE;
			/* The replacement of the directory entry is suspended
			 * until closing time.  The FREPLACE flag indicates
			 * that we expect to do a replace rather than an
			 * append.
			 */
		}
		else {
			/* A bullet file, for reading */
			f->fd_pos = 0;
			f->fd_mode = _S_IFREG | (f->fd_mode & ~_S_IFMT);
			f->fd_flags |= FBULLET;
		}
	}
	else {
		mode_t mode;

		/* File doesn't exist */
		if (!(flags&O_CREAT)) {
			JUNKNEWFD(i, f);
			ERR(ENOENT, "open: file not found");
		} else {
			va_list ap;
#ifdef __STDC__
			va_start(ap, flags);
#else
			va_start(ap);
#endif
			{
				mode = va_arg(ap, mode_t);
			}
			va_end(ap);
		}
		
		/* check that we can install it later on */
		if (!installable(f)) {
			JUNKNEWFD(i, f);
			ERR(EACCES, "open: cannot modify directory");
		}

		/* Create a new bullet file */
		if (get_bullet_cap(&f->fd_cap) != STD_OK) {
			JUNKNEWFD(i, f);
			ERR(EPERM, "open: bullet server not found");
		}

		if (b_create(&f->fd_cap, (char *)NULL, 0, 0, &f->fd_cap)
								!= STD_OK) {
			bulcap_valid = 0; /* relookup next time */
			JUNKNEWFD(i, f);
			ERR(EPERM, "open: b_create failed");
		}
		f->fd_size = 0;
		f->fd_pos = 0;
		f->fd_mode = _S_IFREG | (mode & ~_S_IFMT & ~(_ajax_umask&0777));
		f->fd_flags |= FBULLET | FNONCOMMITTED;
		/* The actual append is suspended until closing time.
		 * Don't set the FREPLACE flag, because when nothing unusual
		 * happens the entry should just be appended.
		 */
	}
	
	if (i < 3)
		_ajax_setstdcap(i, &f->fd_cap);

	/* Make sure we don't lose the file after 10 minutes! */
	if ((f->fd_flags & BFNW) == BFNW)
		_ajax_fd_watchdog();

	RETNEWFD(i, f);
}
