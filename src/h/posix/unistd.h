/*	@(#)unistd.h	1.5	94/04/06 16:54:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <sys/types.h>

/*
** Symbolic Constants mandated by POSIX 1003.1-1988, Section 2.10.
*/

/* Table 2-6. Symbolic Constants for access() */

#define R_OK		4	/* Test for read permission */
#define W_OK		2	/* Test for write permission */
#define X_OK		1	/* Test for execute permission */
#define F_OK		0	/* Test for existence of file */

/* Table 2-7. Symbolic constants for lseek() */

#define SEEK_SET	0	/* Set file offset to argument */
#define SEEK_CUR	1	/* Set file offset to current plus argument */
#define SEEK_END	2	/* Set file offset to EOF plus argument */

/* Table 2-8. Compile-time Symbolic Constants */
/* Only the constants corresponding to supported features are defined */

/* #define _POSIX_JOB_CONTROL	... */
/* #define _POSIX_SAVED_IDS	... */
#define _POSIX_VERSION		199009L	/* POSIX version number (yyyymm) */

/* Table 2-9. Execution-Time Symbolic Constants */
/* Only the constants corresponding to supported features are defined */

#define _POSIX_CHOWN_RESTRICTED	1
	/* Actually, chown is completely unimplementable in Amoeba... */

#define _POSIX_NO_TRUNC		1

#ifndef _POSIX_VDISABLE		/* May also be defined in <termios.h> */
#define _POSIX_VDISABLE		((unsigned char)0xff)
				/* Assumes cc_t in <termios.h> is unsigned */
				/* NB: is the cast allowed here? */
#endif

/* These three definitions are required by POSIX Sec. 8.2.1.2. */
#define STDIN_FILENO		0	/* file descriptor for stdin */
#define STDOUT_FILENO		1	/* file descriptor for stdout */
#define STDERR_FILENO		2	/* file descriptor for stderr */

/* The following relate to configurable system variables. POSIX Table 4-2. */
#define _SC_ARG_MAX		1
#define _SC_CHILD_MAX		2
#define _SC_CLK_TCK		3
#define _SC_NGROUPS_MAX		4
#define _SC_OPEN_MAX		5
#define _SC_JOB_CONTROL		6
#define _SC_SAVED_IDS		7
#define _SC_VERSION		8
#define _SC_STREAM_MAX		9
#define _SC_TZNAME_MAX		10

/* The following relate to configurable pathname variables. POSIX Table 5-2. */
#define _PC_LINK_MAX            1       /* link count */
#define _PC_MAX_CANON           2       /* size of the canonical input queue */
#define _PC_MAX_INPUT           3       /* type-ahead buffer size */
#define _PC_NAME_MAX            4       /* file name size */
#define _PC_PATH_MAX            5       /* pathname size */
#define _PC_PIPE_BUF            6       /* pipe size */
#define _PC_NO_TRUNC            7       /* treatment of long name components */
#define _PC_VDISABLE            8       /* tty disable */
#define _PC_CHOWN_RESTRICTED    9       /* chown restricted or not */

#include "_ARGS.h"

/* the following temporary definitions match the typedefs in <sys/types.h> */
#define __uid_t	int
#define __pid_t	int
#define __gid_t	int
#define __off_t	long

/* From MINIX ... */
void	_exit	_ARGS(( int _status ));
int	access	_ARGS(( char *_path, int _amode ));
int	chdir	_ARGS(( char *_path ));
int	chown	_ARGS(( char *_path, __uid_t _owner, __gid_t _group ));
int	close	_ARGS(( int _fd ));
char *	ctermid _ARGS(( char *_s ));
char *	cuserid _ARGS(( char *_s ));
int 	dup	_ARGS(( int _fd ));
int	dup2	_ARGS(( int _fd, int _fd2 ));
int	execl	_ARGS(( char *_path, ... ));
int	execle	_ARGS(( char *_path, ... ));
int	execlp	_ARGS(( char *_file, ... ));
int	execv	_ARGS(( char *_path, char *_argv[] ));
int	execve	_ARGS(( char *_path, char *_argv[], char *_envp[] ));
int	execvp	_ARGS(( char *_file, char *_argv[] ));
__pid_t	fork	_ARGS(( void ));
long	fpathconf _ARGS(( int _fd, int _name ));
char *	getcwd	_ARGS(( char *_buf, int _size ));
__gid_t	getegid _ARGS(( void ));
__uid_t	geteuid _ARGS(( void ));
__gid_t	getgid	_ARGS(( void ));
int	getgroups _ARGS(( int _gidsetsize, __gid_t _grouplist[] ));
char *	getlogin _ARGS(( void ));
__pid_t	getpgrp	_ARGS(( void ));
__pid_t	getpid	_ARGS(( void ));
__pid_t	getppid	_ARGS(( void ));
__uid_t	getuid	_ARGS(( void ));
unsigned int alarm _ARGS(( unsigned int _seconds ));
unsigned int sleep _ARGS(( unsigned int _seconds ));
int 	isatty	_ARGS(( int _fd ));
int 	link	_ARGS(( const char *_path1, const char *_path2 ));
__off_t	lseek	_ARGS(( int _fd, __off_t _offset, int _whence ));
long	pathconf _ARGS(( char *_path, int _name ));
int	pause	_ARGS(( void ));
int	pipe	_ARGS(( int _fildes[2] ));
ssize_t	read	_ARGS(( int _fd, void *_buf, size_t _n ));
int	rmdir	_ARGS(( const char *_path ));
int	setgid	_ARGS(( __gid_t _gid ));
int	setpgid	_ARGS(( __pid_t _pid, __pid_t _pgid ));
__pid_t	setsid	_ARGS(( void ));
int	setuid	_ARGS(( __uid_t _uid ));
long	sysconf	_ARGS(( int _name ));
__pid_t	tcgetpgrp _ARGS(( int _fd ));
int	tcsetpgrp _ARGS(( int _fd, __pid_t _pgrp_id ));
char *	ttyname	_ARGS(( int _fd ));
int	unlink	_ARGS(( const char *_path ));
ssize_t	write	_ARGS(( int _fd, void *_buf, size_t _n ));

#undef __uid_t
#undef __pid_t
#undef __gid_t
#undef __off_t

/* POSIX 1003.2 declarations... */
#ifdef _POSIX2_SOURCE

int	getopt _ARGS(( int _argc, char * const _argv[], const char *_optstr));
/*
 * according to Posix 1003.2 draft 10, getopt should be declared 
 * int getopt(int argc, const char * const argv[], const char *optstring);
 *                      ^^^^^
 * however this requires the following declaration of main:
 * int main(int argc, const char *argv[]);
 *                    ^^^^^
 * or a cast when calling with argv.
 */

extern char *optarg;
extern int optind, opterr, optopt;

#endif /* _POSIX2_SOURCE */

#endif /* __UNISTD_H__ */
