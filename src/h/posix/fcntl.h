/*	@(#)fcntl.h	1.3	94/04/06 16:53:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix 1003.1 compliant definitions for open(), creat() and fcntl() */

#ifndef __FCNTL_H__
#define __FCNTL_H__

#include "_ARGS.h"

/* 6.5 */

/* Table 6-1.  cmd Values for fcntl() */

#define F_DUPFD		0	/* Duplicate file descriptor */
#define F_GETFD		1       /* Get file descriptor flags */
#define F_SETFD		2       /* Set file descriptor flags */
#define F_GETFL		3       /* Get file status flags */
#define F_SETFL		4       /* Set file status flags */
/* Some values reserved here for BSD */
#define F_GETLK		7	/* Get record locking information */
#define F_SETLK		8	/* Set record locking information */
#define F_SETLKW	9	/* Ditto, wait if blocked */

/* Table 6-2.  File Descriptor Flags Used for fcntl() */

#define FD_CLOEXEC	1	/* Close on exec */

/* Table 6-3.  l_type Values for Record Locking with fcntl() */

#define F_RDLCK		1	/* Shared or read lock */
#define F_UNLCK		2	/* Unlock */
#define F_WRLCK		3	/* Exclusive or write lock */

/* Table 6-4.  oflag Values for open() */

#define O_CREAT		01000	/* Create file if it doesn't exist */
#define O_EXCL		04000	/* Exclusive use flag */
#define O_NOCTTY	02000000 /* Don't assign a controlling terminal */
#define O_TRUNC		02000	/* Truncate flag */

/* Table 6-5.  File Status Flags Used for open() and fcntl() */

#define O_APPEND	010	/* Set append mode */
#define O_NONBLOCK	0400000	/* No delay */

/* Table 6-6.  File Access Modes Used for open() and fcntl() */

#define O_RDONLY	0	/* Open for reading only */
#define O_RDWR		2	/* Open for reading and writing */
#define O_WRONLY	1	/* Open for writing only */

/* Table 6-7. Mask For Use With File Access Modes */

#define O_ACCMODE	3	/* Mask for file access modes */

/* Table 6-8. flock Structure */

struct flock {
	short	l_type;		/* F_RDLCK, F_WRLCK, F_UNLCK */
	short	l_whence;	/* Flag for starting offset */
	long	l_start;	/* Relative offset in bytes (really off_t) */
	long	l_len;		/* Size; if 0 then until EOF (really off_t) */
	int	l_pid;		/* PID of the process holding the lock,
				   returned with F_GETLK (really pid_t) */
};

/* Function prototypes */

int open _ARGS((const char *_path, int _oflag, ...));		    /* 5.3.1 */
int creat _ARGS((const char *_path, /*mode_t*/unsigned short _mode));/* 5.3.2 */
int fcntl _ARGS((int _fildes, int _cmd, ...));			    /* 6.5.2 */

#endif /* __FCNTL_H__ */
