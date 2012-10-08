/*	@(#)am_cmd.c	1.2	96/02/27 10:15:22 */
#ifdef AMOEBA
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <amtools.h>
#include <limits.h>
#include <caplist.h>
#include <termios.h>
#include <semaphore.h>
#include <module/proc.h>

#include "am_defs.h"

static char *login_cmd[] = { "/super/admin/bin/login", NULL };

static char **command_line = &login_cmd[0];

extern capability ttycap;
extern char **environ;
extern struct caplist *capv;

/*
 * Set capability.
 * I made this a function since it cannot be a macro.
 */
void
setcap(capvec, n, name, cap)
    struct caplist *capvec;
    int n;
    char *name;
    capability *cap;
{
    capvec[n].cl_name = name;
    capvec[n].cl_cap = cap;
}

static int
is_iocap_name(capname)
char *capname;
{
    return (strcmp("STDIN",  capname) == 0) ||
	   (strcmp("STDOUT", capname) == 0) ||
	   (strcmp("STDERR", capname) == 0) ||
	   (strcmp("TTY",    capname) == 0);
}

void
kill_session()
{
    TScreen *screen = &term->screen;

    /* If the connection is broken off, make sure the login session
     * is killed as well.
     */
    if (!NULLPORT(&screen->proccap.cap_port)) {
	errstat err;

	if ((err = pro_stun(&screen->proccap, - SIGKILL)) == STD_OK) {
	    warning("killed login session");
	    sleep(5); /* give tty svr a chance to receive it */
	} else {
	    warning("could not kill login session (%s)", err_why(err));
	}
    }
}

/*
 * Fork off the login process.
 */
/*ARGSUSED*/
void
startslave(host, autologin, autoname)
char *host;
int   autologin;
char *autoname;
{
    TScreen *screen = &term->screen;
    int n, ncap;
    errstat err;
    struct caplist *cl, *capvnew;
    capability programcap;

    tty_init();

    /*
     * Setup new capability environment. The whole point of the game is
     * to redirect the shell's stdin/stdout/stderr and tty to our own
     * tty server instead of the initial one.
     */
    for (ncap = 4, cl = capv; cl->cl_name != (char *)NULL; cl++) {
	if (!is_iocap_name(cl->cl_name)) {
	    ncap++;
	}
    }

    capvnew = (struct caplist *)
	calloc((unsigned) ncap + 1, sizeof(struct caplist));
    setcap(capvnew, 0, "STDIN",  &ttycap);
    setcap(capvnew, 1, "STDOUT", &ttycap);
    setcap(capvnew, 2, "STDERR", &ttycap);
    setcap(capvnew, 3, "TTY",    &ttycap);
    for (n = 4, cl = capv; cl->cl_name != (char *) NULL; cl++) {
	if (!is_iocap_name(cl->cl_name)) {
	    setcap(capvnew, n++, cl->cl_name, cl->cl_cap);
	}
    }
    setcap(capvnew, ncap, (char *)NULL, (capability *)NULL);
    if (n != ncap) {
	FatalError("bad capability set.");
    }

    /*
     * Start the login command.
     */
    if ((err = name_lookup(command_line[0], &programcap)) != STD_OK) {
	FatalError("cannot find program %s (%s)",
		   command_line[0], err_why(err));
    }

    if ((err = exec_file(&programcap, NILCAP, &ttycap, 0, command_line,
			 environ, capvnew, &screen->proccap)) != STD_OK)
    {
	FatalError("cannot execute %s (%s)", command_line[0], err_why(err));
    }

    free(capvnew);

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
}

#endif /* AMOEBA */
