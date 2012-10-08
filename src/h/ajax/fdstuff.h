/*	@(#)fdstuff.h	1.5	96/02/29 16:43:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __FDSTUFF_H__
#define __FDSTUFF_H__

/* File descriptor I/O module */

#include <sys/types.h> /* Needed by <sys/stat.h> */
#include <sys/stat.h> /* For S_IFMT etc. */
#include <fcntl.h> /* For O_CREAT etc. */

/* Open file tables.
   The table _ajax_fdmap maps file handles (small integers) to pointers
   to file descriptor blocks (structs describing an open file).
   The low bit is used for the close-on-exec flag.
   The table _ajax_fd contains the actual file descriptor blocks.
   A nil fdmap entry represents a closed file; a zero fd_refcnt member in
   an fd entry indicates an unused descriptor block.  FD blocks contain
   a reference count, so the file can be closed when the last file
   handle referencing it is closed.  (We need a provision for file
   pointer sharing between processes here; this will probably work by
   redirecting all I/O to one process in the group.) */

#define NOFILE OPEN_MAX /* Max open files per process */

#define PATH_NAME_MAX	((PATH_MAX > NAME_MAX) ? PATH_MAX : NAME_MAX)

/* File descriptor block lay-out */
struct fd {
    long	fd_flags;	/* Flags; values taken from <sys/file.h> */
    int		fd_refcnt;	/* Reference count (for dup) */
    int		fd_mode;	/* File mode (for fstat) */
    long	fd_mtime;	/* File mtime (for fstat) */
    long	fd_size;	/* File size */
    long	fd_pos;		/* Position */
    capability	fd_cap;		/* The object's capability */
    capability	fd_dircap;	/* Directory where the object was stored;
				 * working directory when fd_name contains '/'
				 */
    char	fd_name[PATH_NAME_MAX+1];
    				/* The object's name in fd_dircap,
				 * or its full name (read-only optimisation).
				 */
    mutex	fd_mu;		/* For critical sections affecting this fd */
};

/* Flags are mostly taken from <sys/file.h>.
   Allocate new ones from the high end: */
#define FREAD		0x00000001
#define FWRITE		0x00000002
#define FAPPEND		O_APPEND
#define FUNLINK		0x80000000 /* Must be unlinked at close time */
#define FBULLET		0x40000000 /* Bullet file */
#define FNONCOMMITTED	0x20000000 /* Bullet file must be committed */
#define FSTORED		0x10000000 /* Bullet file is put back into directory */
#define FREPLACE	0x08000000 /* Must be replaced rather than appended
				    * at close time */
#define FFIFO		0x04000000 /* fifo */
#define FCREAT		O_CREAT
#define FEXCL		O_EXCL
#define FTRUNC		O_TRUNC
#define FNONBLOCK	O_NONBLOCK

/* Tables */
extern struct fd _ajax_fd[OPEN_MAX]; /* File descriptor blocks */
extern struct fd *_ajax_fdmap[OPEN_MAX]; /* Handle to descriptor block map */
	/* The low bit of fdmap is the close-on-exec flag! */

/* Other variables */
extern mutex _ajax_fdmu; /* Protects critical regions affecting fdmap or fd */
extern int _ajax_fdinited; /* Nonzero when initialization complete */

/* Functions */
void _ajax_fdinit();
int _ajax_newfd();
int *_ajax_idmapping();

/* Macro to initialize */
#define FDINIT() { if (!_ajax_fdinited) _ajax_fdinit(); }

/* Nil file descriptor block */
#define NILFD ((struct fd *)0)

/* Macro pointing to a non-existing, non-nil fd block */
#define NOFD ((struct fd *)1)

/* Macro to get file descriptor block i */
#define FD(i) _ajax_fdmap[i]

/* Macro to remove the close-on-exec bit */
#define CLEANFD(f) ((struct fd *)((long)(f) & ~1L))

/* Macros to get and set the close-on-exec bit */
#define GETCLEXEC(f) ((long)(f) & 1)
#define SETCLEXEC(f, bit) ((struct fd *)((long)(f) | (bit)))

/* Macros to check a file handle and lock it.
   FDLOCK2 executes extra code (e.g., an unlock) before returning an error;
   FDLOCK3 leaves _ajax_fdmu locked if successful. */
#define FDLOCK(i, f) FDLOCK2(i, f, (void *)0)
#define FDLOCK2(i, f, extra) { \
	FDLOCK3(i, f, extra); \
	MU_UNLOCK(&_ajax_fdmu); \
}
#define FDLOCK3(i, f, extra) { \
	if (i < 0 || i >= OPEN_MAX) ERR(EBADF, "FDLOCK: handle out of range"); \
	MU_LOCK(&_ajax_fdmu); \
	if ((f = FD(i)) == NILFD || f == NOFD) { \
		MU_UNLOCK(&_ajax_fdmu); \
		extra; \
		ERR(EBADF, "FDLOCK: invalid handle"); \
	} \
	f = CLEANFD(f); \
	MU_LOCK(&f->fd_mu); \
}

/* Macro to unlock a file handle */
#define FDUNLOCK(f) MU_UNLOCK(&f->fd_mu)

/* Macros to allocate a new file hande; set it to NOFD.
   NEWFD2 starts searching at i instead of at 0;
   NEWFD3 keeps _ajax_fdmu locked if successful. */
#define NEWFD(i) { i = 0; NEWFD2(i); }
#define NEWFD2(i) { \
	NEWFD3(i); \
	MU_UNLOCK(&_ajax_fdmu); \
}
#define NEWFD3(i) { \
	MU_LOCK(&_ajax_fdmu); \
	for (; ; ++i) { \
		if (i >= OPEN_MAX) { \
			MU_UNLOCK(&_ajax_fdmu); \
			ERR(EMFILE, "NEWFD: too many open files"); \
		} \
		if (FD(i) == NILFD) \
			break; \
	} \
	FD(i) = NOFD; \
}

/* Macro to junk a file descriptor block (at close) */
#define JUNKFD(f) { f->fd_refcnt = 0; }

/* Macro to junk a file descriptor block just allocated */
#define JUNKNEWFD(i, f) { FD(i) = NILFD; JUNKFD(f); }

#ifdef UNUSED
/* Macro to unlock and return a file handle */
#define RETFD(i) { \
	FDUNLOCK(FD(i)); \
	return i; \
}
#endif

/* Macro to return a new file handle */
#define RETNEWFD(i, f) { \
	FD(i) = f; \
	return i; \
}

#endif /* __FDSTUFF_H__ */
