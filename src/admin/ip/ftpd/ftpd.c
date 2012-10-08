/*	@(#)ftpd.c	1.1	96/02/27 10:13:57 */
/*
 * Copyright (c) 1985, 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1985, 1988, 1990 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ftpd.c	5.40 (Berkeley) 7/2/91";
#endif /* not lint */

/*
 * FTP server.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef AMOEBA
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif
#include <sys/wait.h>

#ifndef AMOEBA
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#define	FTP_NAMES
#ifndef AMOEBA
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#else
#include <amoeba.h>
#include <stderr.h>
#include <module/name.h>
#include <module/direct.h>
#include <capset.h>
#include <server/soap/soap.h>
#include "ftp.h"
#endif

#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#ifdef AMOEBA
#include <sys/file.h>
#endif
#include <time.h>
#include <pwd.h>
#include <setjmp.h>
#ifndef AMOEBA
#include <netdb.h>
#else
#include <server/ip/gen/netdb.h>
#endif
#include <errno.h>
#include "syslog.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "pathnames.h"

#if defined(__STDC__) || defined(AMOEBA)
#define index(str, c)	strchr(str, c)
#define rindex(str, c)	strrchr(str, c)
#endif

#ifndef NBBY
#define NBBY	8
#endif

/*
 * File containing login names
 * NOT to be used on this machine.
 * Commonly used to disallow uucp.
 */
extern	int errno;
extern	char *crypt();
extern	char version[];
extern	char *home;		/* pointer to home directory for glob */
extern	FILE *ftpd_popen(), *fopen(), *freopen();
extern	int  ftpd_pclose(), fclose();
extern	char *getline();
extern	char cbuf[];
extern	off_t restart_point;

#ifndef AMOEBA
struct	sockaddr_in ctrl_addr;
struct	sockaddr_in data_source;
struct	sockaddr_in data_dest;
struct	sockaddr_in his_addr;
struct	sockaddr_in pasv_addr;
#else
/* Since there is no SIGURG on Amoeba, we have to check in-line */
#ifdef __STDC__
extern volatile int GotTcpUrg;
#else
extern int GotTcpUrg;
#endif
#define check_urg()	if (GotTcpUrg) { GotTcpUrg = 0; myoob(); }
#endif

int	data;
jmp_buf	errcatch, urgcatch;
int	logged_in;
struct	passwd *pw;
int	debug;
#ifdef AMOEBA
#define _timeout _curtimeout      /* avoid conflict with timeout syscall */
#endif
int	timeout = 900;    /* timeout after 15 minutes of inactivity */
int	maxtimeout = 7200;/* don't allow idle time to be set beyond 2 hours */
int	logging;
int	guest;
int	type;
int	form;
int	stru;			/* avoid C keyword */
int	mode;
int	usedefault = 1;		/* for data transfers */
int	pdata = -1;		/* for passive mode */
int	transflag;
off_t	file_size;
off_t	byte_count;
#if !defined(CMASK) || CMASK == 0
#undef CMASK
#define CMASK 027
#endif
int	defumask = CMASK;		/* default umask value */
char	tmpline[7];
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	256
#endif
char	hostname[MAXHOSTNAMELEN];
char	remotehost[MAXHOSTNAMELEN];

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

void	lostconn(), myoob();
#ifndef AMOEBA
FILE	*getdatasock(), *dataconn();
#else
#include "am_conn.h"
extern capability *getcap();
tcpconn *dataconn();
#endif

#ifdef SETPROCTITLE
char	**Argv = NULL;		/* pointer to argument vector */
char	*LastArgv = NULL;	/* end of argv */
char	proctitle[BUFSIZ];	/* initial part of title */
#endif /* SETPROCTITLE */

main(argc, argv, envp)
	int argc;
	char *argv[];
	char **envp;
{
	int addrlen, on = 1, tos;
	char *cp;

	/*
	 * LOG_NDELAY sets up the logging connection immediately,
	 * necessary for anonymous ftp's that chroot and can't do it later.
	 */
	openlog("ftpd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
#ifndef AMOEBA
	addrlen = sizeof (his_addr);
	if (getpeername(0, (struct sockaddr *)&his_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getpeername (%s): %m",argv[0]);
		exit(1);
	}
	addrlen = sizeof (ctrl_addr);
	if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname (%s): %m",argv[0]);
		exit(1);
	}
#ifdef IP_TOS
	tos = IPTOS_LOWDELAY;
	if (setsockopt(0, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	data_source.sin_port = htons(ntohs(ctrl_addr.sin_port) - 1);
#else /* AMOEBA */
	if (!net_init()) {
	    fprintf(stderr, "net_init failed\n");
	    exit(1);
	}
#endif /* AMOEBA */
	debug = 0;
#ifdef SETPROCTITLE
	/*
	 *  Save start and extent of argv for setproctitle.
	 */
	Argv = argv;
	while (*envp)
		envp++;
	LastArgv = envp[-1] + strlen(envp[-1]);
#endif /* SETPROCTITLE */

	argc--, argv++;
	while (argc > 0 && *argv[0] == '-') {
		for (cp = &argv[0][1]; *cp; cp++) switch (*cp) {

		case 'v':
			debug = 1;
			break;

		case 'd':
			debug = 1;
			break;

		case 'l':
			logging = 1;
			break;

		case 't':
			timeout = atoi(++cp);
			if (maxtimeout < timeout)
				maxtimeout = timeout;
			goto nextopt;

		case 'T':
			maxtimeout = atoi(++cp);
			if (timeout > maxtimeout)
				timeout = maxtimeout;
			goto nextopt;

		case 'u':
		    {
			int val = 0;

			while (*++cp && *cp >= '0' && *cp <= '9')
				val = val*8 + *cp - '0';
			if (*cp)
				fprintf(stderr, "ftpd: Bad value for -u\n");
			else
				defumask = val;
			goto nextopt;
		    }

		default:
			fprintf(stderr, "ftpd: Unknown flag -%c ignored.\n",
			     *cp);
			break;
		}
nextopt:
		argc--, argv++;
	}
#ifndef AMOEBA
	(void) freopen(_PATH_DEVNULL, "w", stderr);
	(void) signal(SIGPIPE, lostconn);
	(void) signal(SIGCHLD, SIG_IGN);
	if ((int)signal(SIGURG, myoob) < 0)
		syslog(LOG_ERR, "signal: %m");

	/* Try to handle urgent data inline */
#ifdef SO_OOBINLINE
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0)
		syslog(LOG_ERR, "setsockopt: %m");
#endif

#ifdef	F_SETOWN
	if (fcntl(fileno(stdin), F_SETOWN, getpid()) == -1)
		syslog(LOG_ERR, "fcntl F_SETOWN: %m");
#endif
	dolog(&his_addr);
#endif
	/*
	 * Set up default state
	 */
	data = -1;
	type = TYPE_A;
	form = FORM_N;
	stru = STRU_F;
	mode = MODE_S;
	tmpline[0] = '\0';
	(void) gethostname(hostname, sizeof (hostname));
	reply(220, "%s FTP server (%s) ready.", hostname, version);
	(void) setjmp(errcatch);
	for (;;)
		(void) yyparse();
	/* NOTREACHED */
}

void
lostconn()
{
	if (debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(-1);
}

static char ttyline[20];

/*
 * Helper function for sgetpwnam().
 */
char *
sgetsave(s)
	char *s;
{
	char *new = malloc((unsigned) strlen(s) + 1);

	if (new == NULL) {
		perror_reply(421, "Local resource failure: malloc");
		dologout(1);
		/* NOTREACHED */
	}
	(void) strcpy(new, s);
	return (new);
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
struct passwd *
sgetpwnam(name)
	char *name;
{
	static struct passwd save;
	register struct passwd *p;
	char *sgetsave();

	if ((p = getpwnam(name)) == NULL)
		return (p);
	if (save.pw_name) {
		free(save.pw_name);
		free(save.pw_passwd);
		free(save.pw_gecos);
		free(save.pw_dir);
		free(save.pw_shell);
	}
	save = *p;
	save.pw_name = sgetsave(p->pw_name);
	save.pw_passwd = sgetsave(p->pw_passwd);
	save.pw_gecos = sgetsave(p->pw_gecos);
	save.pw_dir = sgetsave(p->pw_dir);
	save.pw_shell = sgetsave(p->pw_shell);
	return (&save);
}

int login_attempts;		/* number of failed login attempts */
int askpasswd;			/* had user command, ask for passwd */

/*
 * USER command.
 * Sets global passwd pointer pw if named account exists and is acceptable;
 * sets askpasswd if a PASS command is expected.  If logged in previously,
 * need to reset state.  If name is "ftp" or "anonymous", the name is not in
 * _PATH_FTPUSERS, and ftp account exists, set guest and pw, then just return.
 * If account doesn't exist, ask for passwd anyway.  Otherwise, check user
 * requesting login privileges.  Disallow anyone who does not have a standard
 * shell as returned by getusershell().  Disallow anyone mentioned in the file
 * _PATH_FTPUSERS to allow people such as root and uucp to be avoided.
 */
user(name)
	char *name;
{
	register char *cp;
	char *shell;
	char *getusershell();

	if (logged_in) {
		if (guest) {
			reply(530, "Can't change user from guest login.");
			return;
		}
		end_login();
	}

	guest = 0;
#ifndef AMOEBA /* disallow anonymous ftps to and from AMOEBA */
	if (strcmp(name, "ftp") == 0 || strcmp(name, "anonymous") == 0) {
		if (checkuser("ftp") || checkuser("anonymous"))
			reply(530, "User %s access denied.", name);
		else if ((pw = sgetpwnam("ftp")) != NULL) {
			guest = 1;
			askpasswd = 1;
			reply(331, "Guest login ok, send ident as password.");
		} else
			reply(530, "User %s unknown.", name);
		return;
	}
#endif
	if (pw = sgetpwnam(name)) {
		if ((shell = pw->pw_shell) == NULL || *shell == 0)
			shell = _PATH_BSHELL;
		while ((cp = getusershell()) != NULL)
			if (strcmp(cp, shell) == 0)
				break;
		endusershell();
		if (cp == NULL || checkuser(name)) {
			reply(530, "User %s access denied.", name);
			if (logging)
				syslog(LOG_NOTICE,
				    "FTP LOGIN REFUSED FROM %s, %s",
				    remotehost, name);
			pw = (struct passwd *) NULL;
			return;
		}
	}
	reply(331, "Password required for %s.", name);
	askpasswd = 1;
	/*
	 * Delay before reading passwd after first failed
	 * attempt to slow down passwd-guessing programs.
	 */
	if (login_attempts)
		sleep((unsigned) login_attempts);
}

/*
 * Check if a user is in the file _PATH_FTPUSERS
 */
checkuser(name)
	char *name;
{
	register FILE *fd;
	register char *p;
	char line[BUFSIZ];

	if ((fd = fopen(_PATH_FTPUSERS, "r")) != NULL) {
		while (fgets(line, sizeof(line), fd) != NULL)
			if ((p = index(line, '\n')) != NULL) {
				*p = '\0';
				if (line[0] == '#')
					continue;
				if (strcmp(line, name) == 0)
					return (1);
			}
		(void) fclose(fd);
	}
	return (0);
}

/*
 * Terminate login as previous user, if any, resetting state;
 * used when USER command is given or login fails.
 */
end_login()
{

	(void) seteuid((uid_t)0);
	if (logged_in)
		logwtmp(ttyline, "", "");
	pw = NULL;
	logged_in = 0;
	guest = 0;
}

pass(passwd)
	char *passwd;
{
	char *xpasswd, *salt;

	if (logged_in || askpasswd == 0) {
		reply(503, "Login with USER first.");
		return;
	}
	askpasswd = 0;
	if (!guest) {		/* "ftp" is only account allowed no password */
#ifndef AMOEBA
		if (pw == NULL)
			salt = "xx";
		else
			salt = pw->pw_passwd;
		xpasswd = crypt(passwd, salt);
		/* The strcmp does not catch null passwords! */
		if (pw == NULL || *pw->pw_passwd == '\0' ||
		    strcmp(xpasswd, pw->pw_passwd)) {
#else
		if (pw == NULL || !verify_login(pw->pw_name, passwd)) {
#endif
			reply(530, "Login incorrect.");
			pw = NULL;
			if (login_attempts++ >= 5) {
				syslog(LOG_NOTICE,
				    "repeated login failures from %s",
				    remotehost);
				exit(0);
			}
			return;
		}
	}
	login_attempts = 0;		/* this time successful */
	(void) setegid((gid_t)pw->pw_gid);
	(void) initgroups(pw->pw_name, pw->pw_gid);

	/* open wtmp before chroot */
	(void)sprintf(ttyline, "ftp%d", getpid());
	logwtmp(ttyline, pw->pw_name, remotehost);
	logged_in = 1;

	if (guest) {
		/*
		 * We MUST do a chdir() after the chroot. Otherwise
		 * the old current directory will be accessible as "."
		 * outside the new root!
		 */
		if (chroot(pw->pw_dir) < 0 || chdir("/") < 0) {
			reply(550, "Can't set guest privileges.");
			goto bad;
		}
#ifndef AMOEBA
	} else if (chdir(pw->pw_dir) < 0) {
		if (chdir("/") < 0) {
			reply(530, "User %s: can't change directory to %s.",
			    pw->pw_name, pw->pw_dir);
			goto bad;
		} else
			lreply(230, "No directory! Logging in with home=/");
	}
#else
	} else {
		/* Update a few capabilities and capsets causing us
		 * effectively to switch over to the user's environment.
		 */
	        char user_path[256];
		char env_name[256];
		extern capset _sp_rootdir, _sp_workdir;
		static capability user_root;
		capability *capenvp;
		errstat err;

		sprintf(user_path, "/super/users/%s", pw->pw_name);
		err = name_lookup(user_path, &user_root);
		if (err != STD_OK) {
			fprintf(stderr, "cannot lookup %s (%s)\n",
				user_path, err_why(err));
			reply(530, "User %s: can't change directory to %s.",
			    pw->pw_name, pw->pw_dir);
			goto bad;
		}

		if ((capenvp = dir_origin("")) != NULL) {
		    *capenvp = user_root;
		}
		if ((capenvp = dir_origin("/")) != NULL) {
		    *capenvp = user_root;
		}
		cs_singleton(&_sp_rootdir, &user_root);
		cs_singleton(&_sp_workdir, &user_root);
		sprintf(env_name, "USER=%s", pw->pw_name);
		putenv(env_name);
		putenv("_WORK=/");
	}
#endif
#ifndef AMOEBA
	if (seteuid((uid_t)pw->pw_uid) < 0) {
		reply(550, "Can't set uid.");
		goto bad;
	}
#endif
	if (guest) {
		reply(230, "Guest login ok, access restrictions apply.");
#ifdef SETPROCTITLE
		sprintf(proctitle, "%s: anonymous/%.*s", remotehost,
		    sizeof(proctitle) - sizeof(remotehost) -
		    sizeof(": anonymous/"), passwd);
		setproctitle(proctitle);
#endif /* SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "ANONYMOUS FTP LOGIN FROM %s, %s",
			    remotehost, passwd);
	} else {
		reply(230, "User %s logged in.", pw->pw_name);
#ifdef SETPROCTITLE
		sprintf(proctitle, "%s: %s", remotehost, pw->pw_name);
		setproctitle(proctitle);
#endif /* SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "FTP LOGIN FROM %s, %s",
			    remotehost, pw->pw_name);
	}
	home = pw->pw_dir;		/* home dir for globbing */
	(void) umask(defumask);
	return;
bad:
	/* Forget all about it... */
	end_login();
}

#ifdef AMOEBA

extern tcpconn *net_create_conn();
static tcpconn *Cur_dataconn = NULL;

close_dataconn(conn)
tcpconn *conn;
{
    net_flush(conn);
    net_close(conn); /* always equal to Cur_dataconn? */
    Cur_dataconn = NULL;
}

extern char *mktemp();
static char tempfile_templ[30];
static char *tempfile;

static int
close_and_delete_tmp(fp)
FILE *fp;
{
    if (fp != NULL) {
	fclose(fp);
    }

    if (tempfile != NULL) {
	/* TODO: we could destroy it as well */
	remove(tempfile);
    }

    tempfile = NULL;
    return 0;
}

#define MAX_ARGVEC	100
#define IS_SPACE(c)	((c) == ' ' || (c) == '\t')

static char *argvec[MAX_ARGVEC];

int
create_argvec(line, argvp, argcp)
char *line;
char ***argvp;
int  *argcp;
{
    int arg, nargs;
    size_t argsize;
    char *p, *tail, *newarg;

    /* clear previous argvec */
    for (arg = 0; arg < MAX_ARGVEC; arg++) {
	if (argvec[arg] != NULL) {
	    free(argvec[arg]);
	    argvec[arg] = NULL;
	}
    }

    p = line;
    nargs = 0;
    for (arg = 0; arg < MAX_ARGVEC - 1; arg++) {
	for (; *p != 0; p++) {	/* skip leading space */
	    if (!IS_SPACE(*p)) {
		break;
	    }
	}
	if (*p == 0) {		/* end of cmdline */
	    break;
	}

	/* fetch arg */
	for (tail = p + 1; *tail != 0 && !IS_SPACE(*tail); tail++) {
	    /* skip */
	}

	argsize = (tail - p) + 1;
	newarg = (char *) malloc(argsize);
	if (newarg == NULL) {
	    return 0;
	}

	strncpy(newarg, p, argsize - 1);
	newarg[argsize - 1] = 0;
	argvec[nargs++] = newarg;

	p = tail;
    }

    *argcp = nargs;
    *argvp = argvec;
    return 1;
}

int
execute_command(line, outfile)
char *line;
char *outfile;
{
    extern char **environ;
    int    result, status;
    int    fdlist[3];
    char **argv;
    int    argc;
    int    pid;

    if (!create_argvec(line, &argv, &argc)) {
	fprintf(stderr, "%s: cannot build argvec.", line);
	return 0;
    }

    fdlist[0] = 0;
    fdlist[1] = open(outfile, O_WRONLY | O_TRUNC | O_CREAT, 0600);
    if (fdlist[1] < 0) {
	fprintf(stderr, "cannot open %s.", outfile);
	return 0;
    }
    fdlist[2] = fdlist[1];
		
    if ((pid = newproc(argv[0], argv, environ, 3, fdlist, -1L)) < 0) {
	fprintf(stderr, "%s: cannot execute (%d).", line, pid);
	return 0;
    }

    result = waitpid(pid, &status, 0);
    if (result < 0) {
	fprintf(stderr, "%s: wait failed (%d).", line, waitpid);
	return 0;
    }

    close(fdlist[1]);
    return 1;
}

#endif /* AMOEBA */

retrieve(cmd, name)
	char *cmd, *name;
{
#ifndef AMOEBA
	FILE *fin, *dout;
#else
	FILE *fin;
	tcpconn *dout;
#endif
	struct stat st;
	char line[BUFSIZ];
	int (*closefunc)();

	if (cmd == 0) {
		fin = fopen(name, "r"), closefunc = fclose;
		st.st_size = 0;
	} else {
		(void) sprintf(line, cmd, name), name = line;
#ifndef AMOEBA
		fin = ftpd_popen(line, "r"), closefunc = ftpd_pclose;
		st.st_size = -1;
		st.st_blksize = BUFSIZ;
#else
		/* Avoid ftpd_popen which fork()s.  Instead, start
		 * start the command ourselves so that we don't need
		 * a session server for the user himself.
		 * The output is redirected to a tmp file.
		 */
		strcpy(tempfile_templ, "/tmp/ftp_XXXXXX");
		tempfile = mktemp(tempfile_templ);

		if (!execute_command(line, tempfile)) {
		    reply(550, "%s: cannot execute.", line);
		    goto done;
		}

		fin = fopen(tempfile, "r"), closefunc = close_and_delete_tmp;
		cmd = 0; /* we have a plain file, so force stat() below */
		st.st_size = 0;
#endif
	}
	if (fin == NULL) {
		if (errno != 0)
			perror_reply(550, name);
		return;
	}
	if (cmd == 0 &&
	    (fstat(fileno(fin), &st) < 0 || (st.st_mode&S_IFMT) != S_IFREG)) {
		reply(550, "%s: not a plain file.", name);
		goto done;
	}
	if (restart_point) {
		if (type == TYPE_A) {
			register int i, n, c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fin)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}	
		} else if (lseek(fileno(fin), restart_point, L_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	dout = dataconn(name, st.st_size, "w");
	if (dout == NULL)
		goto done;
	send_data(fin, dout, st.st_blksize);
#ifndef AMOEBA
	(void) fclose(dout);
#else
	close_dataconn(dout);
#endif
	data = -1;
	pdata = -1;
done:
	(*closefunc)(fin);
}

store(name, mode, unique)
	char *name, *mode;
	int unique;
{
#ifndef AMOEBA
	FILE *fout, *din;
#else
	FILE *fout;
	tcpconn *din;
#endif
	struct stat st;
	int (*closefunc)();
	char *gunique();

	if (unique && stat(name, &st) == 0 &&
	    (name = gunique(name)) == NULL)
		return;

	if (restart_point)
		mode = "r+w";
	fout = fopen(name, mode);
	closefunc = fclose;
	if (fout == NULL) {
		perror_reply(553, name);
		return;
	}
	if (restart_point) {
		if (type == TYPE_A) {
			register int i, n, c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fout)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}	
			/*
			 * We must do this seek to "current" position
			 * because we are changing from reading to
			 * writing.
			 */
			if (fseek(fout, 0L, L_INCR) < 0) {
				perror_reply(550, name);
				goto done;
			}
		} else if (lseek(fileno(fout), restart_point, L_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	din = dataconn(name, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	if (receive_data(din, fout) == 0) {
		if (unique)
			reply(226, "Transfer complete (unique file name:%s).",
			    name);
		else
			reply(226, "Transfer complete.");
	}
#ifndef AMOEBA
	(void) fclose(din);
#else
	close_dataconn(din);
#endif
	data = -1;
	pdata = -1;
done:
	(*closefunc)(fout);
}

#ifndef AMOEBA

FILE *
getdatasock(mode)
	char *mode;
{
	int s, on = 1, tries;

	if (data >= 0)
		return (fdopen(data, mode));
	(void) seteuid((uid_t)0);
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    (char *) &on, sizeof (on)) < 0)
		goto bad;
	/* anchor socket to avoid multi-homing problems */
	data_source.sin_family = AF_INET;
	data_source.sin_addr = ctrl_addr.sin_addr;
	for (tries = 1; ; tries++) {
		if (bind(s, (struct sockaddr *)&data_source,
		    sizeof (data_source)) >= 0)
			break;
		if (errno != EADDRINUSE || tries > 10)
			goto bad;
		sleep(tries);
	}
	(void) seteuid((uid_t)pw->pw_uid);
#ifdef IP_TOS
	on = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	return (fdopen(s, mode));
bad:
	(void) seteuid((uid_t)pw->pw_uid);
	(void) close(s);
	return (NULL);
}

#endif

#ifndef AMOEBA
FILE *
#else
tcpconn *
#endif
dataconn(name, size, mode)
	char *name;
	off_t size;
	char *mode;
{
	char sizebuf[32];
#ifndef AMOEBA
	FILE *file;
	int retry = 0, tos;
#endif

	file_size = size;
	byte_count = 0;
	if (size != (off_t) -1)
		(void) sprintf (sizebuf, " (%ld bytes)", size);
	else
		(void) strcpy(sizebuf, "");
#ifndef AMOEBA
	if (pdata >= 0) {
		struct sockaddr_in from;
		int s, fromlen = sizeof(from);

		s = accept(pdata, (struct sockaddr *)&from, &fromlen);
		if (s < 0) {
			reply(425, "Can't open data connection.");
			(void) close(pdata);
			pdata = -1;
			return(NULL);
		}
		(void) close(pdata);
		pdata = s;
#ifdef IP_TOS
		tos = IPTOS_LOWDELAY;
		(void) setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
		    sizeof(int));
#endif
		reply(150, "Opening %s mode data connection for %s%s.",
		     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
		return(fdopen(pdata, mode));
	}
	if (data >= 0) {
		reply(125, "Using existing data connection for %s%s.",
		    name, sizebuf);
		usedefault = 1;
		return (fdopen(data, mode));
	}
	if (usedefault)
		data_dest = his_addr;
	usedefault = 1;
	file = getdatasock(mode);
	if (file == NULL) {
		reply(425, "Can't create data socket (%s,%d): %s.",
		    inet_ntoa(data_source.sin_addr),
		    ntohs(data_source.sin_port), strerror(errno));
		return (NULL);
	}
	data = fileno(file);
	while (connect(data, (struct sockaddr *)&data_dest,
	    sizeof (data_dest)) < 0) {
		if (errno == EADDRINUSE && retry < swaitmax) {
			sleep((unsigned) swaitint);
			retry += swaitint;
			continue;
		}
		perror_reply(425, "Can't build data connection");
		(void) fclose(file);
		data = -1;
		return (NULL);
	}
	reply(150, "Opening %s mode data connection for %s%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return (file);
#else /* AMOEBA */
	if (Cur_dataconn != NULL) {
		reply(125, "Using existing data connection for %s%s.",
		    name, sizebuf);
		usedefault = 1;
		return (Cur_dataconn);
	}
	usedefault = 1;
	Cur_dataconn = net_create_conn();
	if (Cur_dataconn == NULL) {
		perror_reply(425, "Can't build data connection");
		return (NULL);
	}
	reply(150, "Opening %s mode data connection for %s%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return Cur_dataconn;
#endif /* AMOEBA */
}

/*
 * Tranfer the contents of "instr" to
 * "outstr" peer using the appropriate
 * encapsulation of the data subject
 * to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
send_data(instr, outstr, blksize)
#ifndef AMOEBA
	FILE *instr, *outstr;
#else
	FILE *instr;
	tcpconn *outstr;
#endif
	off_t blksize;
{
	register int c, cnt;
	register char *buf;
	int netfd, filefd;

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return;
	}
	switch (type) {

	case TYPE_A:
#ifndef AMOEBA
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n') {
				if (ferror(outstr))
					goto data_err;
				(void) putc('\r', outstr);
			}
			(void) putc(c, outstr);
		}
		fflush(outstr);
		transflag = 0;
		if (ferror(instr))
			goto file_err;
		if (ferror(outstr))
			goto data_err;
#else /* AMOEBA */
		while ((c = getc(instr)) != EOF) {
			check_urg();
			byte_count++;
			if (c == '\n') {
				if (net_error(outstr))
					goto data_err;
				(void) net_putc(outstr, '\r');
			}
			(void) net_putc(outstr, c);
		}
		net_flush(outstr);
		transflag = 0;
		if (ferror(instr))
			goto file_err;
		if (net_error(outstr))
			goto data_err;
#endif /* AMOEBA */
		reply(226, "Transfer complete.");
		return;

	case TYPE_I:
	case TYPE_L:
		if ((buf = malloc((u_int)blksize)) == NULL) {
			transflag = 0;
			perror_reply(451, "Local resource failure: malloc");
			return;
		}
#ifndef AMOEBA
		netfd = fileno(outstr);
#endif
		filefd = fileno(instr);
		while ((cnt = read(filefd, buf, (u_int)blksize)) > 0 &&
#ifndef AMOEBA
		    write(netfd, buf, cnt) == cnt)
			byte_count += cnt;
#else
		    net_write(outstr, buf, cnt) == cnt) {
			check_urg();
			byte_count += cnt;
		}
#endif
		transflag = 0;
		(void)free(buf);
		if (cnt != 0) {
			if (cnt < 0)
				goto file_err;
			goto data_err;
		}
		reply(226, "Transfer complete.");
		return;
	default:
		transflag = 0;
		reply(550, "Unimplemented TYPE %d in send_data", type);
		return;
	}

data_err:
	transflag = 0;
	perror_reply(426, "Data connection");
	return;

file_err:
	transflag = 0;
	perror_reply(551, "Error on input file");
}

/*
 * Transfer data from peer to
 * "outstr" using the appropriate
 * encapulation of the data subject
 * to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
receive_data(instr, outstr)
#ifndef AMOEBA
	FILE *instr, *outstr;
#else
	tcpconn *instr;
	FILE *outstr;
#endif
{
	register int c;
	int cnt, bare_lfs = 0;
	char buf[BUFSIZ];

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return (-1);
	}
	switch (type) {

	case TYPE_I:
	case TYPE_L:
#ifndef AMOEBA
		while ((cnt = read(fileno(instr), buf, sizeof buf)) > 0) {
#else
		while ((cnt = net_read(instr, buf, sizeof buf)) > 0) {
			check_urg();
#endif
			if (write(fileno(outstr), buf, cnt) != cnt)
				goto file_err;
			byte_count += cnt;
		}
		if (cnt < 0)
			goto data_err;
		transflag = 0;
		return (0);

	case TYPE_E:
		reply(553, "TYPE E not implemented.");
		transflag = 0;
		return (-1);

	case TYPE_A:
#ifndef AMOEBA
		while ((c = getc(instr)) != EOF) {
#else
		while ((c = net_getc(instr)) != EOF) {
		        sanity_check(instr, "receive_data[1]");
			check_urg();
#endif
			byte_count++;
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				if (ferror(outstr))
					goto data_err;
#ifndef AMOEBA
				if ((c = getc(instr)) != '\n') {
#else
				sanity_check(instr, "receive_data[2]");
				if ((c = net_getc(instr)) != '\n') {
#endif
					(void) putc ('\r', outstr);
					if (c == '\0' || c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, outstr);
	contin2:	;
		}
		fflush(outstr);
#ifndef AMOEBA
		if (ferror(instr))
			goto data_err;
#else
		if (net_error(instr))
			goto data_err;
		sanity_check(instr, "receive_data[3]");
#endif
		if (ferror(outstr))
			goto file_err;
		transflag = 0;
		if (bare_lfs) {
			lreply(230, "WARNING! %d bare linefeeds received in ASCII mode", bare_lfs);
#ifndef AMOEBA
			printf("   File may not have transferred correctly.\r\n");
#else
			net_printf("   File may not have transferred correctly.\r\n");
#endif
		}
		return (0);
	default:
		reply(550, "Unimplemented TYPE %d in receive_data", type);
		transflag = 0;
		return (-1);
	}

data_err:
	transflag = 0;
	perror_reply(426, "Data Connection");
	return (-1);

file_err:
	transflag = 0;
	perror_reply(452, "Error writing file");
	return (-1);
}

statfilecmd(filename)
	char *filename;
{
	char line[BUFSIZ];
	FILE *fin;
	int c;

#ifndef AMOEBA
	(void) sprintf(line, "/bin/ls -lgA %s", filename);
	fin = ftpd_popen(line, "r");
	lreply(211, "status of %s:", filename);
	while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
			if (ferror(stdout)){
				perror_reply(421, "control connection");
				(void) ftpd_pclose(fin);
				dologout(1);
				/* NOTREACHED */
			}
			if (ferror(fin)) {
				perror_reply(551, filename);
				(void) ftpd_pclose(fin);
				return;
			}
			(void) putc('\r', stdout);
		}
		(void) putc(c, stdout);
	}
	(void) ftpd_pclose(fin);
#else
	(void) sprintf(line, "/profile/util/dir -lc %s", filename);
	strcpy(tempfile_templ, "/tmp/ftp_XXXXXX");
	tempfile = mktemp(tempfile_templ);

	if (!execute_command(line, tempfile)) {
	    reply(550, "%s: cannot execute.", line);
	    return;
	}

	fin = fopen(tempfile, "r");
	lreply(211, "status of %s:", filename);
	while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
			if (ferror(fin)) {
				perror_reply(551, filename);
				(void) ftpd_pclose(fin);
				return;
			}
			(void) net_std_putc('\r');
		}
		(void) net_std_putc(c);
	}
	close_and_delete_tmp(fin);
#endif
	reply(211, "End of Status");

}

#ifdef AMOEBA
/* All printf()s to stdout should be written to the client connection,
 * only that doesn't work automatically the way fds are set up in the Amoeba
 * case (and because the TCPIP server doesn't support FS_WRITE requests).
 */
#define printf net_printf
#endif

statcmd()
{
#ifndef AMOEBA
	struct sockaddr_in *sin;
#endif
	u_char *a, *p;

	lreply(211, "%s FTP server status:", hostname, version);
	printf("     %s\r\n", version);
	printf("     Connected to %s", remotehost);
#ifndef AMOEBA
	if (!isdigit(remotehost[0]))
		printf(" (%s)", inet_ntoa(his_addr.sin_addr));
#endif
	printf("\r\n");
	if (logged_in) {
		if (guest)
			printf("     Logged in anonymously\r\n");
		else
			printf("     Logged in as %s\r\n", pw->pw_name);
	} else if (askpasswd)
		printf("     Waiting for password\r\n");
	else
		printf("     Waiting for user name\r\n");
	printf("     TYPE: %s", typenames[type]);
	if (type == TYPE_A || type == TYPE_E)
		printf(", FORM: %s", formnames[form]);
	if (type == TYPE_L)
#if NBBY == 8
		printf(" %d", NBBY);
#else
		printf(" %d", bytesize);	/* need definition! */
#endif
	printf("; STRUcture: %s; transfer MODE: %s\r\n",
	    strunames[stru], modenames[mode]);
	if (data != -1)
		printf("     Data connection open\r\n");
	else if (pdata != -1) {
		printf("     in Passive mode");
#ifndef AMOEBA
		sin = &pasv_addr;
#endif
		goto printaddr;
	} else if (usedefault == 0) {
		printf("     PORT");
#ifndef AMOEBA
		sin = &data_dest;
printaddr:
		a = (u_char *) &sin->sin_addr;
		p = (u_char *) &sin->sin_port;
#else
printaddr:
		a = (u_char *) &data_dest_addr;
		p = (u_char *) &data_dest_port;
#endif
#define UC(b) (((int) b) & 0xff)
		printf(" (%d,%d,%d,%d,%d,%d)\r\n", UC(a[0]),
			UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
#undef UC
	} else
		printf("     No data connection\r\n");
	reply(211, "End of status");
}

fatal(s)
	char *s;
{
	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
	/* NOTREACHED */
}

/* VARARGS2 */
reply(n, fmt, p0, p1, p2, p3, p4, p5)
	int n;
	char *fmt;
{
	printf("%d ", n);
	printf(fmt, p0, p1, p2, p3, p4, p5);
	printf("\r\n");
#ifndef AMOEBA
	(void)fflush(stdout);
#else
	net_std_flush();
#endif
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d ", n);
		syslog(LOG_DEBUG, fmt, p0, p1, p2, p3, p4, p5);
	}
}

/* VARARGS2 */
lreply(n, fmt, p0, p1, p2, p3, p4, p5)
	int n;
	char *fmt;
{
	printf("%d- ", n);
	printf(fmt, p0, p1, p2, p3, p4, p5);
	printf("\r\n");
#ifndef AMOEBA
	(void)fflush(stdout);
#else
	net_std_flush(stdout);
#endif
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d- ", n);
		syslog(LOG_DEBUG, fmt, p0, p1, p2, p3, p4, p5);
	}
}

ack(s)
	char *s;
{
	reply(250, "%s command successful.", s);
}

nack(s)
	char *s;
{
	reply(502, "%s command not implemented.", s);
}

/* ARGSUSED */
yyerror(s)
	char *s;
{
	char *cp;

	if (cp = index(cbuf,'\n'))
		*cp = '\0';
	reply(500, "'%s': command not understood.", cbuf);
}

delete(name)
	char *name;
{
	struct stat st;

	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return;
	}
	if ((st.st_mode&S_IFMT) == S_IFDIR) {
		if (rmdir(name) < 0) {
			perror_reply(550, name);
			return;
		}
		goto done;
	}
	if (unlink(name) < 0) {
		perror_reply(550, name);
		return;
	}
done:
	ack("DELE");
}

cwd(path)
	char *path;
{
	if (chdir(path) < 0)
		perror_reply(550, path);
	else
		ack("CWD");
}

makedir(name)
	char *name;
{
	if (mkdir(name, 0777) < 0)
		perror_reply(550, name);
	else
		reply(257, "MKD command successful.");
}

removedir(name)
	char *name;
{
	if (rmdir(name) < 0)
		perror_reply(550, name);
	else
		ack("RMD");
}

pwd()
{
	char path[MAXPATHLEN + 1];
	extern char *getwd();

	if (getwd(path) == (char *)NULL)
		reply(550, "%s.", path);
	else
		reply(257, "\"%s\" is current directory.", path);
}

char *
renamefrom(name)
	char *name;
{
	struct stat st;

	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return ((char *)0);
	}
	reply(350, "File exists, ready for destination name");
	return (name);
}

renamecmd(from, to)
	char *from, *to;
{
	if (rename(from, to) < 0)
		perror_reply(550, "rename");
	else
		ack("RNTO");
}

#ifndef AMOEBA

dolog(sin)
	struct sockaddr_in *sin;
{
	struct hostent *hp = gethostbyaddr((char *)&sin->sin_addr,
		sizeof (struct in_addr), AF_INET);
	time_t t, time();
	extern char *ctime();

	if (hp)
		(void) strncpy(remotehost, hp->h_name, sizeof (remotehost));
	else
		(void) strncpy(remotehost, inet_ntoa(sin->sin_addr),
		    sizeof (remotehost));
#ifdef SETPROCTITLE
	sprintf(proctitle, "%s: connected", remotehost);
	setproctitle(proctitle);
#endif /* SETPROCTITLE */

	if (logging) {
		t = time((time_t *) 0);
		syslog(LOG_INFO, "connection from %s at %s",
		    remotehost, ctime(&t));
	}
}

#endif /* AMOEBA */

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
dologout(status)
	int status;
{
	if (logged_in) {
		(void) seteuid((uid_t)0);
		logwtmp(ttyline, "", "");
	}
	/* beware of flushing buffers after a SIGPIPE */
	_exit(status);
}

void
myoob()
{
	char *cp;

	/* only process if transfer occurring */
	if (!transflag)
		return;
	cp = tmpline;
	if (getline(cp, 7, stdin) == NULL) {
		reply(221, "You could at least say goodbye.");
		dologout(0);
	}
	upper(cp);
	if (strcmp(cp, "ABOR\r\n") == 0) {
		tmpline[0] = '\0';
		reply(426, "Transfer aborted. Data connection closed.");
		reply(226, "Abort successful");
		longjmp(urgcatch, 1);
	}
	if (strcmp(cp, "STAT\r\n") == 0) {
		if (file_size != (off_t) -1)
			reply(213, "Status: %lu of %lu bytes transferred",
			    byte_count, file_size);
		else
			reply(213, "Status: %lu bytes transferred", byte_count);
	}
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 * 	the PASV command in RFC959. However, it has been blessed as
 * 	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
passive()
{
	int len;
	register char *p, *a;

#ifndef AMOEBA
	pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}
	pasv_addr = ctrl_addr;
	pasv_addr.sin_port = 0;
	(void) seteuid((uid_t)0);
	if (bind(pdata, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) < 0) {
		(void) seteuid((uid_t)pw->pw_uid);
		goto pasv_error;
	}
	(void) seteuid((uid_t)pw->pw_uid);
	len = sizeof(pasv_addr);
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr, &len) < 0)
		goto pasv_error;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	a = (char *) &pasv_addr.sin_addr;
	p = (char *) &pasv_addr.sin_port;

#define UC(b) (((int) b) & 0xff)

	reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
		UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
	return;

pasv_error:
	(void) close(pdata);
#endif /* AMOEBA */
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * Generate unique name for file with basename "local".
 * The file named "local" is already known to exist.
 * Generates failure reply on error.
 */
char *
gunique(local)
	char *local;
{
	static char new[MAXPATHLEN];
	struct stat st;
	char *cp = rindex(local, '/');
	int count = 0;

	if (cp)
		*cp = '\0';
	if (stat(cp ? local : ".", &st) < 0) {
		perror_reply(553, cp ? local : ".");
		return((char *) 0);
	}
	if (cp)
		*cp = '/';
	(void) strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	for (count = 1; count < 100; count++) {
		(void) sprintf(cp, "%d", count);
		if (stat(new, &st) < 0)
			return(new);
	}
	reply(452, "Unique file name cannot be created.");
	return((char *) 0);
}

/*
 * Format and send reply containing system error number.
 */
perror_reply(code, string)
	int code;
	char *string;
{
	reply(code, "%s: %s.", string, strerror(errno));
}

static char *onefile[] = {
	"",
	0
};

send_file_list(whichfiles)
	char *whichfiles;
{
	struct stat st;
	DIR *dirp = NULL;
	struct dirent *dir;
#ifndef AMOEBA
	FILE *dout = NULL;
#else
	tcpconn *dout = NULL;
#endif
	register char **dirlist, *dirname;
	int simple = 0;
	char *strpbrk();

	if (strpbrk(whichfiles, "~{[*?") != NULL) {
		extern char **ftpglob(), *globerr;

		globerr = NULL;
		dirlist = ftpglob(whichfiles);
		if (globerr != NULL) {
			reply(550, globerr);
			return;
		} else if (dirlist == NULL) {
			errno = ENOENT;
			perror_reply(550, whichfiles);
			return;
		}
	} else {
		onefile[0] = whichfiles;
		dirlist = onefile;
		simple = 1;
	}

	if (setjmp(urgcatch)) {
		transflag = 0;
		return;
	}
	while (dirname = *dirlist++) {
		if (stat(dirname, &st) < 0) {
			/*
			 * If user typed "ls -l", etc, and the client
			 * used NLST, do what the user meant.
			 */
			if (dirname[0] == '-' && *dirlist == NULL &&
			    transflag == 0) {
				retrieve("/bin/ls %s", dirname);
				return;
			}
			perror_reply(550, whichfiles);
			if (dout != NULL) {
#ifndef AMOEBA
				(void) fclose(dout);
#else
				close_dataconn(dout);
#endif
				transflag = 0;
				data = -1;
				pdata = -1;
			}
			return;
		}

		if ((st.st_mode&S_IFMT) == S_IFREG) {
			if (dout == NULL) {
				dout = dataconn("file list", (off_t)-1, "w");
				if (dout == NULL)
					return;
				transflag++;
			}
#ifndef AMOEBA
			fprintf(dout, "%s%s\n", dirname,
				type == TYPE_A ? "\r" : "");
#else
			net_fprintf(dout, "%s%s\n", dirname,
				type == TYPE_A ? "\r" : "");
#endif
			byte_count += strlen(dirname) + 1;
			continue;
		} else if ((st.st_mode&S_IFMT) != S_IFDIR)
			continue;

		if ((dirp = opendir(dirname)) == NULL)
			continue;

		while ((dir = readdir(dirp)) != NULL) {
			char nbuf[MAXPATHLEN];

#ifndef AMOEBA
			if (dir->d_name[0] == '.' && dir->d_namlen == 1)
				continue;
			if (dir->d_name[0] == '.' && dir->d_name[1] == '.' &&
			    dir->d_namlen == 2)
				continue;
#else
			check_urg();
			if (strcmp(dir->d_name, "." ) == 0 ||
			    strcmp(dir->d_name, "..") == 0)
				continue;
#endif


			sprintf(nbuf, "%s/%s", dirname, dir->d_name);

			/*
			 * We have to do a stat to insure it's
			 * not a directory or special file.
			 */
			if (simple || (stat(nbuf, &st) == 0 &&
			    (st.st_mode&S_IFMT) == S_IFREG)) {
				if (dout == NULL) {
					dout = dataconn("file list", (off_t)-1,
						"w");
					if (dout == NULL)
						return;
					transflag++;
				}
#ifndef AMOEBA
				if (nbuf[0] == '.' && nbuf[1] == '/')
					fprintf(dout, "%s%s\n", &nbuf[2],
						type == TYPE_A ? "\r" : "");
				else
					fprintf(dout, "%s%s\n", nbuf,
						type == TYPE_A ? "\r" : "");
#else
				if (nbuf[0] == '.' && nbuf[1] == '/')
					net_fprintf(dout, "%s%s\n", &nbuf[2],
						type == TYPE_A ? "\r" : "");
				else
					net_fprintf(dout, "%s%s\n", nbuf,
						type == TYPE_A ? "\r" : "");
#endif
				byte_count += strlen(nbuf) + 1;
			}
		}
		(void) closedir(dirp);
	}

	if (dout == NULL)
		reply(550, "No files found.");
#ifndef AMOEBA
	else if (ferror(dout) != 0)
#else
	else if (net_error(dout) != 0)
#endif
		perror_reply(550, "Data connection");
	else
		reply(226, "Transfer complete.");
	transflag = 0;
#ifndef AMOEBA
	if (dout != NULL)
		(void) fclose(dout);
#else
	if (dout != NULL)
		close_dataconn(dout);
#endif
	data = -1;
	pdata = -1;
}

#ifdef SETPROCTITLE
/*
 * clobber argv so ps will show what we're doing.
 * (stolen from sendmail)
 * warning, since this is usually started from inetd.conf, it
 * often doesn't have much of an environment or arglist to overwrite.
 */

/*VARARGS1*/
setproctitle(fmt, a, b, c)
char *fmt;
{
	register char *p, *bp, ch;
	register int i;
	char buf[BUFSIZ];

	(void) sprintf(buf, fmt, a, b, c);

	/* make ps print our process name */
	p = Argv[0];
	*p++ = '-';

	i = strlen(buf);
	if (i > LastArgv - p - 2) {
		i = LastArgv - p - 2;
		buf[i] = '\0';
	}
	bp = buf;
	while (ch = *bp++)
		if (ch != '\n' && ch != '\r')
			*p++ = ch;
	while (p < LastArgv)
		*p++ = ' ';
}
#endif /* SETPROCTITLE */
