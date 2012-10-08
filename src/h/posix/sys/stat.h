/*	@(#)stat.h	1.4	94/04/06 16:56:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __STAT_H__
#define __STAT_H__

/* Posix 1003.1 compliant definitions for stat.h */

#include "_ARGS.h"

struct	stat {
	dev_t	st_dev;
	ino_t	st_ino;
	mode_t	st_mode;
	nlink_t	st_nlink;
	uid_t	st_uid;
	gid_t	st_gid;
	dev_t	st_rdev;
	off_t	st_size;
	time_t	st_atime;
	long	_st_spare1;	/* For BSD lay-out compatibility */
	time_t	st_mtime;
	long	_st_spare2;	/* For BSD lay-out compatibility */
	time_t	st_ctime;
	long	_st_spare3;	/* For BSD lay-out compatibility */
	
	/* Extra members for BSD compatibility: */
	long	st_blksize;
	long	st_blocks;
	long	_st_spare4[2];
};

/* 5.6.1.1. File Types */

#define _S_IFMT			0170000
#define		_S_IFDIR	0040000
#define		_S_IFCHR	0020000
#define		_S_IFBLK	0060000
#define		_S_IFREG	0100000
#define		_S_IFIFO	0010000

#define S_ISDIR(m)	(((m) & _S_IFMT) == _S_IFDIR)
#define S_ISCHR(m)	(((m) & _S_IFMT) == _S_IFCHR)
#define S_ISBLK(m)	(((m) & _S_IFMT) == _S_IFBLK)
#define S_ISREG(m)	(((m) & _S_IFMT) == _S_IFREG)
#define S_ISFIFO(m)	(((m) & _S_IFMT) == _S_IFIFO)

/* 5.6.1.2 File Modes. */

#define S_IRWXU			0700
#define		S_IRUSR		0400
#define		S_IWUSR		0200
#define		S_IXUSR		0100

#define S_IRWXG			0070
#define		S_IRGRP		0040
#define		S_IWGRP		0020
#define		S_IXGRP		0010

#define S_IRWXO			0007
#define		S_IROTH		0004
#define		S_IWOTH		0002
#define		S_IXOTH		0001

#define S_ISUID			04000
#define S_ISGID			02000


#ifndef _POSIX_SOURCE

/* BSD-compatible symbols: */

#define S_IFMT			_S_IFMT
#define		S_IFDIR		_S_IFDIR
#define		S_IFCHR		_S_IFCHR
#define		S_IFBLK		_S_IFBLK
#define		S_IFREG		_S_IFREG

#define S_IFLNK			0120000
#define S_IFSOCK		0140000

#define S_ISVTX			0001000
#define S_IREAD			S_IRUSR
#define S_IWRITE		S_IWUSR
#define S_IEXEC			S_IXUSR

#endif /* _POSIX_SOURCE */


/* Function prototypes */

mode_t	umask	_ARGS((mode_t _cmask));			/* 5.3.3 */
int	mkdir	_ARGS((char *_path, mode_t _mode));	/* 5.4.1 */
int	mkfifo	_ARGS((char *_path, mode_t _mode));	/* 5.4.2 */
int	stat	_ARGS((char *_path, struct stat *_buf));/* 5.6.2 */
int	fstat	_ARGS((int _fildes, struct stat *_buf));/* 5.6.2 */
int	chmod	_ARGS((char *_path, mode_t _mode));	/* 5.6.4 */

#endif /* __STAT_H__ */
