/*	@(#)wait.h	1.6	94/04/06 16:57:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __WAIT_H__
#define __WAIT_H__

/* Posix 1003.1 compliant wait() and waitpid() interface */

#include "_ARGS.h"

/* 3.2.1 Wait for Process Termination */

/*
** We use the traditional Unix lay-out of the wait status:
**
**		low byte	high byte
**
** exited	0		exit status
** signal	signal number	0
** stopped	0x7f		signal number
**
** In fact, the high 3 bytes give the exit status, and if right shifts
** on ints sign-extend, negative status values are possible.
**
** The value 0x80 is or-ed into the signal number when a successful
** core dump was made.
*/

#define WNOHANG		1	/* do not wait for child to exit */
#define WUNTRACED	2	/* for job control (not supported) */

#define WIFEXITED(x)	( ((x) & 0xff) == 0 )
#define WEXITSTATUS(x)	( ((x) & ~0xff) >> 8 )
#define WIFSIGNALED(x)	( ((x) & 0xff) != 0 && !WIFSTOPPED(x) )
#define WTERMSIG(x)	( (x) & 0x7f )
#define WIFSTOPPED(x)	( ((x) & 0xff) == 0x7f )
#define WSTOPSIG(x)	( (x) >> 8 )

/* Function prototypes */

int wait _ARGS((int *_stat_loc));
int waitpid _ARGS((pid_t _pid, int *_stat_loc, int _options));


#ifndef _POSIX_SOURCE
/* Needed for wait3() emulation. */
union wait {
	int w_status;
};
#endif

#endif /* __WAIT_H__ */
