/*	@(#)environ.c	1.6	96/02/27 10:16:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * environ.c
 *
 * This module provides a basic interface for login programs (like login
 * and xlogin). All the functions essentially parse the contents of the
 * /Environ file and try to execute a shell. If this fails it is up to
 * the login program to start a default environment (see login.c).
 *
 * Functions:
 *
 * int verify_login(u, p) char *u, *p;
 *	This function verifies whether user ``u'' can login using password
 *	``p''. Access is determined by checking the passwd field in a non-
 *	secure environment, or by query-ing the login/authentication server
 *	in a secure environment.
 *
 * int setup_environ(u) char *u;
 *	Setup a basic string and capability environment for user ``u''. Some
 *	capabilities and strings are taken from the callers environments and
 *	some are created.
 *
 * int read_environ(u) char *u;
 *	Parse and execute the environment file "/Environ" for user ``u''.
 *	This function may not return at all when an exec command is encountered
 *	or can return true or false depending on the parsing state. In either
 *	case the caller should start a default environment (shell).
 *
 * void execute(v) char **v;
 *	Execute command with its arguments. The argument list is terminated
 *	by a NULL pointer. This routine either returns in case it can recover
 *	or exits with an appropriate exit code in case it cannot.
 *
 * void putstrenv(n, s) char *n, *s;
 *	Put name/string pair in new string environment.
 *
 * char *getstrenv(n) char *n;
 *	Get string belonging to name from the new string environment.
 *
 * void putcapenv(n, c) char *n; capability *c;
 *	Put name/capability pair in new capability environment.
 *
 * capability *getcapenv(n) char *n;
 *	Get capability belonging to name from the new capability environment.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amtools.h>
#include <ampolicy.h>
#include <module/proc.h>

/* tunable constants */
#define	DEFAULT_SHELL	"/bin/sh"
#define	DEFAULT_PATH	"/bin:/profile/util"
#define	MAX_ARGUMENTS	20
#define	MAX_LINE	2048
#define	USERS_DIR	"/super/users"
#define	THRESHOLD	20

/* manifest constants */
#define	TRUE		1
#define	FALSE		0

#define	is_blank(ch)	((ch)==' ' || (ch)=='\t' || (ch)=='\n' || (ch)=='\r')

int warn = FALSE;			/* print warnings (like parse errors) */
int debug = FALSE;			/* no debug output to begin with */

static capability nullcap;

static int numstrenv = 0;		/* # of entries in string environment */
static int strenvsize = 0;		/* total size of string environment */
static char **strenviron = NULL;	/* actual string environment */

static int numcapenv = 0;		/* # of entries in cap environment */
static int capenvsize = 0;		/* total size of cap environment */
static struct caplist *capenviron;	/* actual capability environment */

static int do_setenv(), do_setcap(), do_exec();

static struct table {
    char *name;
    int (*func)();
} table[] = {
    { "setenv",	do_setenv },
    { "setcap",	do_setcap },
#ifdef LOGIN
    { "exec",	do_exec   },
#else
    { "xexec",	do_exec   },
#endif
};

void		execute();
char		*getstrenv();
void		putstrenv();
capability	*getcapenv();
void		putcapenv();

static FILE	*open_environ();
static void	close_environ();
static int	read_line();

/*
 * Get password entry from /Environ file, and login.
 * No passwd entry means, unfortunately, access.
 */
static int verify_password(/* passwd, encrypted */);

int
verify_login(user, pass)
    char *user, *pass;
{
    char *argvec[MAX_ARGUMENTS];
    FILE *fp;

    if ((fp = open_environ(user)) == NULL)
	return FALSE;
    while (read_line(fp, argvec)) {
	/* format: passwd [<encrypted-password>] */
	if (strcmp(argvec[0], "passwd") == 0) {
	    if (!argvec[2]) {
		close_environ(fp);
	        return argvec[1] ? verify_password(pass, argvec[1]) : TRUE;
	    }
	}
    }
    close_environ(fp);
    (void)verify_password(pass, "\0\0\0\0\0\0\0\0\0\0\0\0");
    return FALSE;
}

static int
verify_password(passwd, encrypted)
    char *passwd, *encrypted;
{
    char salt[3], *crypt();

    salt[0] = encrypted[0];
    salt[1] = encrypted[1];
    salt[2] = '\0';
    return strcmp(encrypted, crypt(passwd, salt)) == 0;
}

/*
 * Setup default environment (based on our environment)
 */
int
setup_environ(user)
    char *user;
{
    capability slash, home, *ttycap;
    register char *p;
    char dir[BUFSIZ];
    errstat err;

    /*
     * String environment variables
     */
    putstrenv("ROOT", "/");
    putstrenv("_WORK", "/home");
    putstrenv("HOME", "/home");
    putstrenv("USER", user);
    putstrenv("LOGNAME", user);
    putstrenv("PATH", "/bin:/profile/util:.");
    if ((p = getenv("TERM")) != NULL)
	putstrenv("TERM", p);

    /*
     * Capability environment variables
     */
    sprintf(dir, "%s/%s", USERS_DIR, user);
    if ((err = name_lookup(dir, &slash)) != STD_OK) {
	printf("Cannot lookup slash capability (%s)\n", err_why(err));
	return FALSE;
    }
    putcapenv("ROOT", &slash);
    sprintf(dir, "%s/%s/home", USERS_DIR, user);
    if ((err = name_lookup(dir, &home)) != STD_OK) {
	printf("Cannot lookup home capability (%s)\n", err_why(err));
	return FALSE;
    }
    putcapenv("HOME", &home);
    putcapenv("WORK", &home);

    /*
     * Capability environment variables. The STDIN, STDOUT, and
     * STDERR capabilities are only exported when there is a
     * TTY capability in our environment. So for X, the program
     * that's executed ought to set up a tty server (see xterm).
     */
    if ((ttycap = getcapenv("TTY")) != NILCAP) {
	putcapenv("STDIN", ttycap);
	putcapenv("STDOUT", ttycap);
	putcapenv("STDERR", ttycap);
    }

    return TRUE;
}

/*
 * Read "/Environ" and interpret its options
 */
int
read_environ(user)
    char *user;
{
    char *argvec[MAX_ARGUMENTS];
    FILE *fp;
    int i;

    if ((fp = open_environ(user)) == NULL)
	return FALSE;
    while (read_line(fp, argvec)) {
	if (debug) {
	    printf("Environ:");
	    for (i = 0; i < MAX_ARGUMENTS && argvec[i]; i++)
		printf(" %s", argvec[i]);
	    printf("\n");
	}
	for (i = 0; i < sizeof(table)/sizeof(struct table); i++) {
	    if (argvec[0] && strcmp(table[i].name, argvec[0]) == 0) {
		if (!(*table[i].func)(argvec)) {
		    close_environ(fp);
		    return FALSE;
		}
	    }
	    continue;
	}
    }
    close_environ(fp);
    return TRUE;
}

/*
 * Open ~user/Environ file to read password
 * and (eventually) its environment from.
 */
static FILE *
open_environ(user)
    char *user;
{
    char environ[BUFSIZ];
    FILE *fp;

    strncpy(environ, USERS_DIR, BUFSIZ);
    strncat(environ, "/", BUFSIZ);
    strncat(environ, user, BUFSIZ);
    strncat(environ, "/", BUFSIZ);
    strncat(environ, ENVIRONFILE, BUFSIZ);
    if ((fp = fopen(environ, "r")) == NULL && debug)
	fprintf(stderr, "Cannot open environment file (%s)\n", environ);
    return fp;
}

static void
close_environ(fp)
    FILE *fp;
{
    fclose(fp);
}

/*
 * Read Environment line. Each line contains a function specifier
 * followed by an optional number of argument. When a hash (#) appears
 * at the beginning of a line, it remained is taken as comment.
 */
static int
read_line(fp, argv)
    FILE *fp;
    char **argv;
{
    static char line[MAX_LINE];
    int ch, n, i;

    /* zero argument vector, to prevent races */
    for (i = 0; i < MAX_ARGUMENTS; i++)
	argv[i] = NULL;

    /* skip comment */
    while ((ch = getc(fp)) == '#') {
	do
	    ch = getc(fp);
	while (ch != '\n' && ch != EOF);
	if (ch == EOF) return FALSE;
    }
    if (ch == EOF) return FALSE;

    /* parse line, storing each argument in argv */
    i = n = 0;
    while (ch != '\n' && ch != EOF) {
	while (ch == ' ' || ch == '\t')
	    ch = getc(fp);
	if (ch == '\n' || ch == EOF) break;
	if (n >= MAX_ARGUMENTS - 1)
	    return FALSE;
	argv[n++] = &line[i];
	while (ch != EOF && !is_blank(ch)) {
	    if (i >= MAX_LINE - 1)
		return FALSE;
	    line[i++] = ch;
	    ch = getc(fp);
	}
	line[i++] = '\0';
    }
    argv[n] = NULL;
    return TRUE;
}

/*
 * Store name/value pair in the child's string environment
 */
static int
do_setenv(argv)
    char **argv;
{
    if (!argv[1] || !argv[2]) {
	if (warn) fprintf(stderr, "Invalid setenv format\n");
	return FALSE;
    }
    putstrenv(argv[1], argv[2]);
    return TRUE;
}

/*
 * Lookup capability and store name/capability pair in
 * capability environment of the child process.
 */
static errstat lookup(/* name, cap */);

static int
do_setcap(argv)
    char **argv;
{
    capability cap;
    errstat err;

    if (!argv[1] || !argv[2]) {
	if (warn) fprintf(stderr, "Invalid setcap format\n");
	return FALSE;
    }
    if ((err = lookup(argv[2], &cap)) != STD_OK) {
	if (warn) fprintf(stderr, "Lookup of %s failed (%s)\n",
	    argv[2], err_why(err));
	return FALSE;
    }
    putcapenv(argv[1], &cap);
    return TRUE;
}

/*
 * Execute command vector. Start default command if execute fails.
 */
static int
do_exec(argv)
    char **argv;
{
    execute(argv + 1);
    return TRUE;
}

/*
 * Execute a process. The command to be executed is taken from
 * the argument vector. Any directory lookups or references to
 * environment variables are taken from the child's environments.
 */
static errstat find_program(/* program, programcap */);

void
execute(argv)
    char **argv;
{
    capability proccap, newproc;
    capability runcap, pooldir;
    capability programcap;
    char *shell, *argvec[4];
    process_d pd;
    errstat err;

    if (debug) {
        struct caplist *q;
        char **p;

        for (p = strenviron; p && *p; p++)
            printf("STRING ENVIRONMENT: %s\n", *p);
        for (q = capenviron; q && q->cl_name; q++)
            printf("CAP ENVIRONMENT: %s %s\n", q->cl_name, ar_cap(q->cl_cap));
	for (p = argv; *p; p++)
	    printf("EXEC_SHELL: %s\n", *p);
    }

    /*
     * Get the owner of the login process
     */
    if (getinfo(&proccap, &pd, sizeof(pd)) < 0) {
	if (warn) fprintf(stderr, "Cannot find my owner\n");
	return;
    }

    /*
     * Specify the users's pool directory & run server to the exec
     * library routine, since it may be different that of this program
     */
    if ((err = lookup(DEF_RUNSVR_POOL, &runcap)) != STD_OK)
	runcap = nullcap; /* fall back */
    if ((err = lookup(POOL_DIR, &pooldir)) != STD_OK) {
	if (warn) fprintf(stderr, "Cannot lookup pool directory %s (%s)\n",
	    POOL_DIR, err_why(err));
	return;
    }
    exec_set_runsvr(&runcap);
    exec_set_pooldir(&pooldir);

    /*
     * Lookup program and execute it
     */
    if ((err = find_program(argv[0], &programcap)) != STD_OK) {
	if (warn) fprintf(stderr, "Cannot find %s (%s)\n",
	    argv[0], err_why(err));
	return;
    }
    err = exec_file(&programcap, NILCAP, &pd.pd_owner, 0,
	    argv, strenviron, capenviron, &newproc);
    if (err != STD_OK) {
	if (warn) fprintf(stderr, "Could not execute %s (%s)\n",
	    argv[0], err_why(err));
	return;
    }

    /*
     * Unregister ourself, and register the command just executed
     * at the parent (our owner).
     */
    if ((err = pro_setowner(&proccap, &nullcap)) != STD_OK) {
	if (warn) fprintf(stderr, "Cannot become an orphan (%s)\n",
	    err_why(err));
	exit(1);
    }
    err = pro_swapproc(&pd.pd_owner, &proccap, &newproc);
    if (err != RPC_BADPORT && err != STD_OK)
	if (warn) fprintf(stderr, "Warning: swap owner failed (%s)\n",
	    err_why(err));
    exit(0);
}


/*
 * Find process descriptor for specified program,
 * necessarily running down the user's PATH.
 */
static errstat
find_program(program, programcap)
    char *program;
    capability *programcap;
{
    errstat err;

    if ((err = lookup(program, programcap)) != STD_OK) {
	char *path, *name;
	char programpath[1024];

	if ((path = getstrenv("PATH")) == NULL)
     	    path = DEFAULT_PATH;
	if ((name = strrchr(program, '/')) != NULL)
    	    name++;
	else
    	    name = program;

	do {
	    register char *p = programpath;
	    register char *n = name;
	    char *c1 = path;

	    while (*path && *path != ':')
		*p++ = *path++;
	    if (path != c1) *p++ = '/';
	    if (*path) path++;
	    while (*n) *p++ = *n++;
	    *p = '\0';
	    if ((err = lookup(programpath, programcap)) == STD_OK)
		break;
	} while (*path);
    }
    return err;
}

/*
 * Lookup provides the same functionality as name_lookup does, but
 * everything (i.e. references to capenv) is relative to the child's
 * environment.
 */
static errstat
lookup(name, cap)
    char *name;
    capability *cap;
{
    register capability *start;

    /* determine start directory */
    if (*name == '/') {
	do ++name; while (*name == '/');
	start = getcapenv("ROOT");
    } else
	start = getcapenv("HOME");
    if (cap == NILCAP) return STD_SYSERR;

    if (*name == '\0') {
	*cap = *start;
	return STD_OK;
    }
    return dir_lookup(start, name, cap);
}

/*
 * Add name/string pair to string environment
 */
void
putstrenv(name, string)
    char *name, *string;
{
    char *p, **q, *s, buffer[BUFSIZ];

    /* store name/string pair as <name>=<string> */
    strcpy(buffer, name);
    strcat(buffer, "=");
    strcat(buffer, string);
    if ((p = malloc(strlen(buffer) + 1)) == NULL)
	return;
    strcpy(p, buffer);

    /* first determine whether it's already there */
    for (q = strenviron; q && *q != NULL; q++) {
	if ((s = strchr(*q, '=')) != NULL) {
	    if (strncmp(name, *q, s - *q) == 0) {
		free(*q);
		*q = p;
		return;
	    }
	}
    }

    /* allocate new space, when old one is exhausted */
    if (numstrenv >= strenvsize-1) {
	if (strenviron == NULL) {
	    strenvsize = THRESHOLD;
	    strenviron = (char **) malloc(strenvsize * sizeof(char **));
	} else {
	    strenvsize += THRESHOLD;
	    strenviron = (char **) realloc(strenviron,
		strenvsize * sizeof(char **));
	}
	if (strenviron == NULL) {
	    strenvsize = numstrenv = 0;
	    return;
	}
    }

    strenviron[numstrenv++] = p;
    strenviron[numstrenv] = NULL;
}

/*
 * Get string for name from string environment
 */
char *
getstrenv(name)
    char *name;
{
    register char **p, *s;

    for (p = strenviron; p && *p != NULL; p++) {
	if ((s = strchr(*p, '=')) != NULL) {
	    if (strncmp(name, *p, s - *p) == 0)
		return s + 1;
	}
    }
    return NULL;
}

/*
 * Add name/capability pair to capability environment
 */
void
putcapenv(name, cap)
    char *name;
    capability *cap;
{
    register struct caplist *p;
    struct caplist cl;

    /* first determine whether it's already there */
    for (p = capenviron; p && p->cl_name; p++) {
	if (strcmp(name, p->cl_name) == 0) {
	    if (!p->cl_cap)
		p->cl_cap = (capability *) malloc(sizeof(capability));
	    if (p->cl_cap)
		*p->cl_cap = *cap;
	    return;
	}
    }

    /* allocate new space, when old one is exhausted */
    if (numcapenv >= capenvsize-1) {
	if (capenviron == NULL) {
	    capenvsize = THRESHOLD;
	    capenviron = (struct caplist *)
		malloc(capenvsize * sizeof(struct caplist));
	} else {
	    capenvsize += THRESHOLD;
	    capenviron = (struct caplist *) realloc(capenviron,
		capenvsize * sizeof(struct caplist));
	}
	if (capenviron == NULL) {
	    capenvsize = numcapenv = 0;
	    return;
	}
    }

    /* add name/capability pair to capability environment */
    cl.cl_name = malloc(strlen(name) + 1);
    if (cl.cl_name == NULL) return;
    strcpy(cl.cl_name, name);
    cl.cl_cap = (capability *) malloc(sizeof(capability));
    if (cl.cl_cap == NILCAP) return;
    *cl.cl_cap = *cap;
    capenviron[numcapenv++] = cl;
    capenviron[numcapenv].cl_name = NULL;
}

/*
 * Get capability for name from capability environment
 */
capability *
getcapenv(name)
    char *name;
{
    register struct caplist *p;

    for (p = capenviron; p && p->cl_name; p++) {
	if (strcmp(name, p->cl_name) == 0)
	    return p->cl_cap;
    }
    return NULL;
}
