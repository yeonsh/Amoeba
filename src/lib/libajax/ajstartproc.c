/*	@(#)ajstartproc.c	1.6	96/02/27 10:58:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Start a process.  Code shared by execve and newproc. */

#include "ajax.h"
#include "sesstuff.h"
#include "fdstuff.h"
#include "sigstuff.h"
#include <caplist.h>
#include "am_exerrors.h"
#include "bullet/bullet.h"
#include "module/proc.h"
#include <ctype.h>


extern char **environ;

/*
 * Command script execution support.
 * If a file that doesn't contain a valid process descriptor starts with
 * "#!", we assume it to be a script file.
 * The first line of that file should be of the form
 *
 * "#! cmd [cmdarg] \n"
 *
 * The argument vector is changed from "script arg1 .. argn"
 * to "cmd cmdarg scriptpath arg1 .. argn".
 */

/* Maximum number of chars to read from the first line of a script. */
#define MAXLINELEN 256

#define space_or_tab(c)	((c) == ' ' || (c) == '\t')

static int
fetch_string(buf, after, str)
char *buf;
char **after;
char **str;
/*
 * Returns in `str' a pointer to an allocated copy of the first string
 * in `buf'.  Strings in `buf' are seperated by spaces or tabs.
 * If there are no more strings in `buf', `str' is set to NULL.
 *
 * In case of errors, this function returns 0.  Otherwise it returns 1,
 * and `after' is pointed to the remaining part of the buffer.
 */
{
    register char *cp, *start, *result;

    /* Find start of string, skipping leading space characters */
    cp = buf;
    while (space_or_tab(*cp)) {
	cp++;
    }
    start = cp;

    /* determine result */
    if (*cp == '\0') {
	/* we're already at the and of the buffer */
	result = NULL;
    } else {
	size_t size;

	/* Find end of string, by skipping non-space characters */
	while (*cp != '\0' && !space_or_tab(*cp)) {
	    cp++;
	}
    
	/* Allocate string */
	size = cp - start + 1;
	if ((result = (char *)malloc(size)) == NULL) {
	    return 0;	/* allocation failure */
	}
	strncpy(result, start, size - 1);
	result[size - 1] = '\0';
    }

    *str = result;
    *after = cp;
    return 1;
}

static int
read_interpreter (file, cmd_ret, cmdarg_ret)
capability *file;
char	  **cmd_ret;
char	  **cmdarg_ret;
/*
 * If `file' is a script, return in `cmd' to the name of the program
 * that should run it.  If `file' specifies an argument for the program,
 * `cmdarg' will be pointed to it; otherwise `cmdarg' is set to NULL.
 * Return 1 if successful, 0 if not.
 */
{
    char     line[MAXLINELEN + 1];
    char    *curpos, *newline;
    b_fsize  nread;
    errstat  err;
    char    *cmd = NULL, *cmdarg = NULL;
#   define   Failure(why)	{ goto failure; /* free() allocated mem */ }

    err = b_read(file, (b_fsize) 0, line, (b_fsize) MAXLINELEN, &nread);
    if (err != STD_OK) {
	Failure("could not read script");
    }

    if (nread < 2 || line[0] != '#' || line[1] != '!') {
	Failure("not a #! script");
    }

    /* make sure `line' is terminated properly */
    line[nread] = '\0';
    if ((newline = strchr(line, '\n')) == NULL) {
	Failure("script line too long");
    }
    *newline = '\0';

    /* Fetch command name following "#!" */
    curpos = line + 2;
    if (fetch_string(curpos, &curpos, &cmd) == 0) {
	Failure("could not get interpreter name");
    }

    if (cmd == NULL) {
	/* Command name is lacking; default to /bin/sh */
	static char bourne_shell[] = "/bin/sh";

	if ((cmd = (char *)malloc(sizeof bourne_shell)) == NULL) {
	    Failure("out of memory");
	}
	strcpy(cmd, bourne_shell);
    } else {
	/* We succesfully parsed "#! command"; now lets see if it has an
	 * argument.  Arguments following it are ignored.
	 */
	if (fetch_string(curpos, &curpos, &cmdarg) == 0) {
	    Failure("could not get argument");
	}
    }

    *cmd_ret = cmd;
    *cmdarg_ret = cmdarg;
    return 1;

failure:
    if (cmd != NULL) free(cmd);
    if (cmdarg != NULL) free(cmdarg);
    return 0;
}

static int
exec_script(script_path, cmd, cmdarg, session, argv, envp, newcapv, process)
char		*script_path;
char		*cmd;
char		*cmdarg;
capability	*session;
char	       **argv;
char	       **envp;
struct caplist	*newcapv;
capability	*process;
/*
 * From the null-terminated argument list `argv', create a new argument list
 * containing `cmd' and, if non-NULL, `cmdarg', and `script_path'.
 * Then try to run `cmd' with the new argument list and `envp'.
 * Returns an AMEX error status or STD_OK.
 */
{
    register char **new_argv;
    register char **ap;
    register int cur_argc = 0, argc = 0;
    errstat err;

    /* determine current argv length */
    for (ap = argv; *ap != NULL; ++ap) {
	cur_argc++;
    }
    
    /* build new argument list */
    new_argv = (char **)malloc((size_t)((cur_argc + 4) * sizeof(char *)));
    if (new_argv == 0) {
	return AMEX_MALLOC;
    }

    /* first insert new arguments */
    new_argv[argc++] = cmd;
    if (cmdarg != NULL) {
	new_argv[argc++] = cmdarg;
    }
    new_argv[argc++] = script_path;

    /* copy other arguments from argv, skipping the first one */
    for (ap = argv + 1; *ap != NULL; ++ap) {
	new_argv[argc++] = *ap;
    }
    new_argv[argc++] = NULL;

    /* let exec_file() look up the program to use from new_argv[0] */
    err = exec_file((capability *)NULL, (capability *)NULL,
		    session, 0 /*default stacksize*/,
		    new_argv, envp, newcapv, process);
    free(new_argv);
    return err;
}


#define MAXCAPS 200

int
_ajax_startproc(isnewproc, file, argv, envp, nfd, fdlist, sigignore)
	int isnewproc; /* nonzero if called from newproc */
	char *file;
	char *argv[];
	char *envp[];
	int nfd;
	int fdlist[];
	long sigignore;
{
	capability filecap;
	capability process;
	capability *session;
	capability shared[NOFILE];
	int nshared;
	capability newowner;
	struct caplist newcapv[MAXCAPS];
	char buf[1024];
	errstat err;
	
	if (name_lookup(file, &filecap) != STD_OK)
		ERR(ENOENT, "startproc: file not found");
	if (envp == NULL)
		envp = environ;
	if (nfd < 0) {
		nfd = NOFILE;
		fdlist = _ajax_idmapping();
	}
	if (sigignore < 0)
		sigignore = _ajax_sigignore;
	SESINIT(session); /* Connect to session server */
	MU_LOCK(&_ajax_forkmu); /* No other process mgmt calls please... */
	if (isnewproc) { /* newproc */
		_ajax_share(nfd, fdlist);
		nshared = _ajax_getshared(nfd, fdlist, shared, NOFILE);
		/* Get an owner cap for the new child */
		err = ses_newchild(session, sigignore, _ajax_sigblock,
				_ajax_sigpending, &newowner, shared, nshared);
		session = &newowner;
	}
	else { /* execve */
		_ajax_commitall(); /* Commit writable bullet files */
				   /* (Needed for close-on-exec files) */
		/* Tell session server we are about to exec a process */
		err = ses_preexec(session, sigignore,
				  _ajax_sigblock, _ajax_sigpending);
	}
	if (err != STD_OK) {
		MU_UNLOCK(&_ajax_forkmu);
		_ajax_putnum("Ajax (exec): session svr failure, error", err);
		ERR(EIO, "startproc: preexec call failed");
	}
	_ajax_makecapv(nfd, fdlist, newcapv, MAXCAPS, buf, sizeof buf);
	err = exec_file(&filecap, NILCAP, session, 0 /*default stacksize*/,
			argv, envp, newcapv, &process);

	if (err == AMEX_PDREAD) {
		/* In that case it might be a script file instead */
		char *cmd, *cmdarg;

		if (read_interpreter(&filecap, &cmd, &cmdarg)) {
			err = exec_script(file, cmd, cmdarg, session,
					  argv, envp, newcapv, &process);
			free((_VOIDSTAR)cmd);
			free((_VOIDSTAR)cmdarg);
		}
	}

	if (err != STD_OK) { /* exec_file() failed */
		(void) ses_execbad(session);
		MU_UNLOCK(&_ajax_forkmu);
		switch (err) {
		case AMEX_PDREAD:
		case AMEX_PDSIZE:
			ERR(ENOEXEC, "startproc: bad pd");
		case AMEX_NOPROG:
		case AMEX_PDLOOKUP:
			ERR(ENOEXEC, "startproc: bad script");
		case AMEX_NOHOST:
			ERR(EAGAIN,  "startproc: no suitable host");
		case AMEX_MALLOC:
		case AMEX_STACKOV:
		case AMEX_SEGCREATE:
			ERR(ENOMEM, "startproc: stack problems");
		case STD_NOMEM:
		case STD_NOSPACE:
			ERR(ENOMEM, "startproc: no space on host");
		default:
			ERR(EIO,
			    strcat(strcpy(buf, "startproc: "), err_why(err)));
		}
	}
	/* Tell session server about it */
	(void) ses_execok(session, &process);
	if (!isnewproc) { /* execve */
		capability self;
		static capability nullcap;
		(void) _ajax_execlose(); /* Close files with close/exec bit */
		/* Clear the owner so we don't bother the session server
		   with the checkpoint from exitprocess() */
		if (getinfo(&self, NILPD, 0) == 0)
			(void) pro_setowner(&self, &nullcap);
		exitprocess(-1);
		/*NOTREACHED*/
	}
	MU_UNLOCK(&_ajax_forkmu);
	return prv_number(&session->cap_priv); /* Return new process's pid */
}
