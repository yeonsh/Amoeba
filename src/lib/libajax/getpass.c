/*	@(#)getpass.c	1.3	96/02/27 11:30:47 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <sys/types.h>
#include <signal.h>
#include <sgtty.h>
#include <string.h>
#include <ampolicy.h>

char * getpass(prompt)
char *prompt;
{
	int i = 0;
	struct sgttyb tty, ttysave;
	static char pwdbuf[MAX_PASSWD_SZ + 1];
	int fd;
	void (*savesig)();

	if ((fd = open("/dev/tty", 0)) < 0) fd = 0;
	savesig = signal(SIGINT, SIG_IGN);
	write(2, prompt, strlen(prompt));
	gtty(fd, &tty);
	ttysave = tty;
	tty.sg_flags &= ~(ECHO | ECHOK | ECHONL);
	stty(fd, &tty);
	i = read(fd, pwdbuf, MAX_PASSWD_SZ + 1);
	if (i > 0)
	{
	    while (pwdbuf[i - 1] != '\n')
		    if (read(fd, &pwdbuf[i - 1], 1) <= 0)
			    break;
	    pwdbuf[i - 1] = '\0';
	}
	else
	    pwdbuf[0] = '\0';
	stty(fd, &ttysave);
	write(2, "\n", 1);
	if (fd != 0) close(fd);
	signal(SIGINT, savesig);
	return(pwdbuf);
}
