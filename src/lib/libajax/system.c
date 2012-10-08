/*	@(#)system.c	1.4	94/04/07 09:54:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix / ANSI C system() function, using Ajax' newproc() function */

#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>

extern char **environ;

#define SHELL "/bin/sh"

int
system(str)
	char *str;
{
	static char shell[] = SHELL;
	static char *argv[4] = {"sh", "-c"};
	static int fdlist[3] = {0, 1, 2};
	int pid, exitstatus, waitval;
	void (*saveint)(), (*savequit)();

	if (str == 0) {
		/* ANSI invented that system(NULL) test if system() works.
		   We stat the shell to see if we stand a chance. */
		struct stat sb;
		return stat(shell, &sb) == 0 && S_ISREG(sb.st_mode) &&
					((sb.st_mode & 0x111) != 0);
	}

	argv[2] = str;
	if ((pid = newproc(shell, argv, environ, 3, fdlist, -1L)) < 0)
		return 127 << 8; /* See man pages for wait() and system() */
	saveint = signal(SIGINT, SIG_IGN);
	savequit = signal(SIGQUIT, SIG_IGN);
	waitval = waitpid(pid, &exitstatus, 0);
	if (waitval < 0) {
		/* no child ??? or maybe interrupted ??? */
		exitstatus = -1;
	}
	signal(SIGINT, saveint);
	signal(SIGQUIT, savequit);
	return exitstatus;
}
