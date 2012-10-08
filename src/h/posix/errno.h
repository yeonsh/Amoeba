/*	@(#)errno.h	1.3	96/02/27 10:34:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ERRNO_H__
#define __ERRNO_H__

/* Posix 1003.1 compliant error numbers */

/* 2.5 Error Numbers */

extern int errno;

#define E2BIG		7		/* Arg list too long */
#define EACCES		13		/* Permission denied */
#define EAGAIN		11		/* Resource temporarily unavailable */
#define EBADF		9		/* Bad file descriptor */
#define EBUSY		16		/* Resource busy */
#define ECHILD		10		/* No child processes */
#define EDEADLK		35		/* Operation would block */
#define EDOM		33		/* Domain error */
#define EEXIST		17		/* File exists */
#define EFAULT		14		/* Bad address */
#define EFBIG		27		/* File too large */
#define EINTR		4		/* Interrupted system call */
#define EINVAL		22		/* Invalid argument */
#define EIO		5		/* Input/output error */
#define EISDIR		21		/* Is a directory */
#define EMFILE		24		/* Too many open files */
#define EMLINK		31		/* Too many links */
#define ENAMETOOLONG	63		/* File name too long */
#define ENFILE		23		/* Too many open files in system */
#define ENODEV		19		/* No such device */
#define ENOENT		2		/* No such file or directory */
#define ENOEXEC		8		/* Exec format error */
#define ENOLCK		80		/* No locks available */
#define ENOMEM		12		/* Not enough core */
#define ENOSPC		28		/* No space left on device */
#define ENOSYS		81		/* Function not implemented */
#define ENOTDIR		20		/* Not a directory */
#define ENOTEMPTY	66		/* Directory not empty */
#define ENOTTY		25		/* Inappropriate ioctl operation */
#define ENXIO		6		/* No such device or address */
#define EPERM		1		/* Operation not permitted */
#define EPIPE		32		/* Broken pipe */
#define ERANGE		34		/* Result too large */
#define EROFS		30		/* Read-only file system */
#define ESPIPE		29		/* Invalid seek */
#define ESRCH		3		/* No such process */
#define	EXDEV		18		/* Cross device link */

#endif /* __ERRNO_H__ */
