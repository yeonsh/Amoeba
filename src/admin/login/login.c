/*	@(#)login.c	1.4	96/02/27 10:16:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * login.c
 *
 * The Amoeba login program. Almost a trivial application with the
 * help of environ.c.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <bool.h>
#include <stdio.h>
#include <sgtty.h>
#include <amtools.h>
#include <version.h>
#include <posix/termios.h>
#include <class/tios.h>
#include <ampolicy.h>	/* For MAX_LOGNAME_SZ, etc */

#ifndef DEFAULT_SHELL
#define	DEFAULT_SHELL	"/bin/sh"
#endif
#ifndef DEFAULT_VMIN
#define	DEFAULT_VMIN	1
#endif
#ifndef DEFAULT_VTIME
#define	DEFAULT_VTIME	0
#endif

#ifndef MAX_LOGNAME_SZ
#define	MAX_LOGNAME_SZ	20
#endif
#ifndef MAX_PASSWD_SZ
#define	MAX_PASSWD_SZ	8
#endif

extern int warn;		/* enable warning output */
extern int debug;		/* enable debug output */

extern char *optarg;
extern int optind;

/*
 * Default shell command
 */
char *default_shell[] = {
    "/profile/util/session",
    "-a",
    DEFAULT_SHELL,
    NULL
};

char *getstrenv();
char *getuser();
char *getpass();
void usage();

main(argc, argv)
    int argc;
    char **argv;
{
    char *shell, *user, *pass;
    struct termios tios;
    capability *ttycap;
    int banner, opt;
    errstat err;

    warn = TRUE;
    debug = FALSE;
    banner = TRUE;
    user = 0;
    while ((opt = getopt(argc, argv, "bdw")) != EOF) {
	switch (opt) {
	case 'b': /* disable banner */
	    banner = FALSE;
	    break;
	case 'd': /* enable debug output */
	    debug = TRUE;
	    break;
	case 'w': /* disable warning messages */
	    warn = FALSE;
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (optind < argc) {
	banner = FALSE;
	user = argv[optind];
    }

    /*
     * Pass on the tty capability for child's standard
     * input, output, and error stream.
     */
    if ((ttycap = getcap("TTY")) == NILCAP) {
	fprintf(stderr, "No tty capability (huh?)\n");
	exit(1);
    }
    putcapenv("TTY", ttycap);

    /*
     * Put terminal in some reasonable default values.
     * The values are the same as sty reset. Note that
     * signals are turned of so that this program can
     * run without the help of the session server.
     */
    tios.c_iflag = BRKINT|ICRNL|IXOFF|IXON;
    tios.c_oflag = OPOST;
    tios.c_cflag = CS8|B9600|CREAD;
    tios.c_lflag = ECHO|ECHOE|ECHOK|ICANON;
    tios.c_cc[VEOF] = 'd' & 0x1f;
    tios.c_cc[VEOL] = _POSIX_VDISABLE;
    tios.c_cc[VERASE] = 'h' & 0x1f;
    tios.c_cc[VINTR] = 'c' & 0x1f;
    tios.c_cc[VKILL] = 'x' & 0x1f;
    tios.c_cc[VQUIT] = '\\' & 0x1f;
    tios.c_cc[VSUSP] = _POSIX_VDISABLE;
    tios.c_cc[VSTART] = 'q' & 0x1f;
    tios.c_cc[VSTOP] = 's' & 0x1f;
    tios.c_cc[VMIN] = DEFAULT_VMIN;
    tios.c_cc[VTIME] = DEFAULT_VTIME;
    if ((err = tios_setattr(ttycap, TCSANOW, &tios)) != STD_OK) {
	fprintf(stderr, "%s: failed to reset tty: %s\n",
	    argv[0], err_why(err));
	exit(1);
    }

    /*
     * Get user name and password and determine whether he/she
     * can access the system.
     */
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    for (;;) {
	if (!user) {
	    if (banner)
		fprintf(stderr, "\n%s login\n\n", AMOEBA_VERSION);
	    do {
		user = getuser("Username: ");
	    } while (!user || !user[0]);
	}
	pass = getpass("Password: ");
	if (verify_login(user, pass))
	    break;
	fprintf(stderr, "Incorrect user name or password\n\n");
	user = NULL;
    }

    /*
     * Turn on the signals. We've come far enough to make
     * this just a simple warning.
     */
    tios.c_lflag |= ISIG;
    if ((err = tios_setattr(ttycap, TCSANOW, &tios)) != STD_OK)
	fprintf(stderr, "%s: warning: failed to reset tty: %s\n",
	    argv[0], err_why(err));

    /*
     * Try to parse the /Environ file. When it fails or when there
     * is no exec in the /Environ file (in that case read_environ
     * returns) we'll use our own default environment.
     */
    if (!setup_environ(user)) {
	fprintf(stderr, "Cannot setup default environment\n");
	exit(1);
    }
    if (!read_environ(user))
	fprintf(stderr, "Syntax error in environment file; using defaults.\n");

    if ((shell = getstrenv("SHELL")) != NULL)
	default_shell[2] = shell;
    execute(default_shell);
    exit(0);
}

void
usage(program)
    char *program;
{
    fprintf(stderr, "Usage: %s [-b] [-w] [user name]\n", program);
    exit(1);
}

char *
getuser(prompt)
    char *prompt;
{
    static char ubuf[MAX_LOGNAME_SZ + 1];
    register char *p;
    register int ch = '\0';

    fputs(prompt, stderr);
    if (feof(stdin) || ferror(stdin)) clearerr(stdin);
    for (p = ubuf; (ch = getc(stdin)) != '\n' && ch != '\r' && ch != EOF; )
	if (p < &ubuf[MAX_LOGNAME_SZ])
	    *p++ = (char) ch;
    *p = '\0';
    if (ch == EOF) fputs("\n", stderr);
    return ubuf;
}

/*
 * We don't use the library version since it needs a session server
 * and we may not have one!
 */
char *
getpass(prompt)
    char *prompt;
{
    static char pbuf[MAX_PASSWD_SZ + 1];
    struct sgttyb tty, ttysave;
    register char *p;
    register int ch;

    fputs(prompt, stderr);
    if (feof(stdin) || ferror(stdin)) clearerr(stdin);
    gtty(fileno(stdin), &ttysave);
    tty = ttysave;
    tty.sg_flags &= ~ECHO;
    stty(fileno(stdin), &tty);
    for (p = pbuf; (ch = getc(stdin)) != '\n' && ch != '\r' && ch != EOF; )
	if (p < &pbuf[MAX_PASSWD_SZ])
	    *p++ = (char) ch;
    *p = '\0';
    stty(fileno(stdin), &ttysave);
    fputs("\n", stderr);
    return pbuf;
}
