/*	@(#)newproc.c	1.2	94/04/07 09:49:15 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Newproc is intended to replace the common case of a fork followed
   (in the child) by some I/O redirection and an exec system call.
   
   The first three arguments correspond to the arguments to execve.
   
   The arguments nfd and fdlist specify I/O redirection.  If nfd is
   negative, the child will inherit all open files from the parent
   execpt those with the close-on-exec flag on.  Otherwise, for
   0 <= i < nfd, file descriptor i in the child will be dupped from file
   descriptor fdlist[i] in the parent, if that refers to a valid file
   descriptor whose close-on-exec flag os off, and closed otherwise;
   file descriptors nfd and higher in the child are closed.
   
   The last argument is a signal mask: signals corresponding to one bits
   in the mask will initially be ignored by the child, while the default
   action will be used for the others.  (Remember that signal i
   corresponds to bit i-1, as defined by the sigmask() macro in
   <signal.h>.)
   
   The return value is the process ID of the new process.  This is a
   child of the calling process, indistinguishable from one created by
   fork and exec.  If no process was started for some reason, -1 is
   returned.
   
   (We probably need a 'path' version of this, and maybe there
   are more pieces of process state deserving a parameter:
   umask, various limits, uids/gids, access groups, root and
   current dir, process group, controlling tty, timers and
   (Amoeba) capability environment...)

*/

#include "ajax.h"

int
newproc(file, argv, envp, nfd, fdlist, sigignore)
	char *file;
	char *argv[];
	char *envp[];
	int nfd;
	int fdlist[];
	long sigignore;
{
	return _ajax_startproc(1, file, argv, envp, nfd, fdlist, sigignore);
}
