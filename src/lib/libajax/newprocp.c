/*	@(#)newprocp.c	1.3	94/04/07 09:49:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Variant on newproc that uses $PATH, like execvp */

/* TO DO: only continue at certain errors (not at EIO?) */

#include "ajax.h"

int
newprocp(file, argv, envp, nfd, fdlist, sigignore)
	char *file;
	char *argv[];
	char *envp[];
	int nfd;
	int fdlist[];
	long sigignore;
{
	char buf[1024];
	register char *path;
	register char *p;
	int pid;
	
	if (strchr(file, '/'))
		path = "";
	else {
		if ((path = getenv("PATH")) == NULL)
			path = ":/bin:/usr/bin";
	}
	for (;;) {
		p = buf;
		while (*path != '\0' && *path != ':')
			*p++ = *path++;
		if (p != buf)
			*p++ = '/';
		strcpy(p, file);
		pid = _ajax_startproc(1, buf, argv, envp, nfd, fdlist,
								sigignore);
		if (pid >= 0 || *path == '\0')
			return pid;
		++path;
	}
}
