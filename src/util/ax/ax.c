/*	@(#)ax.c	1.8	96/02/27 13:07:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**   ax [options] program [argument] ...
**
** Execute an Amoeba program from Unix.
** Options are explained at individual cases.
**
** The program must be stored on an Amoeba file server in standard
** Amoeba process format; this is a process descriptor followed by a
** standard Unix a.out header and is normally created by ainstall.
*/

/* Unix include files */
#include "sys/types.h"
#include "signal.h"
#include "ctype.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#ifdef SYSV
#include "unistd.h"
#endif

/* Amoeba include files */
#include "amoeba.h"
#ifdef UNIX
#include "host_os.h" /* For SIGAMOEBA */
#else
#include "ajax/ajax.h"
#endif
#include "cmdreg.h"
#include "stderr.h"
#include "caplist.h"
#include "module/name.h"
#include "ampolicy.h"
#include "module/direct.h"
#include "module/proc.h"
#include "module/host.h"
#include "module/rnd.h"
#include "module/prv.h"

/* Pointer to the UNIX environment and a new environ for the Amoeba prog: */
extern char ** environ;
char *         am_envp[1] = { NULL };	/* An empty environment */
char **        am_environ = am_envp;
unsigned int   am_env_alloc = 0;	/* Indicate to env_put() that
					 * am_environ is not a malloc'd block
					 */
/* From the Amoeba library */
extern char **      env_put();

/* Getopt interface */
extern int	optind;
extern char *	optarg;

/* Some pathnames and environment names used by ax */
#define HOME_DIR	"/"		/* Default $HOME */
#define HOME_ENV	"HOME"
#define HOST_ENV	"AX_HOST"
#define HOST_CAPENV	"AX_HOST"
#define SESSION_CAPENV	"_SESSION"


/* Capability list set up */
#define NCAP		100
struct caplist		CapList[NCAP+1];

/* Last argument to cap2list */
#define NO_OVERRIDE	0
#define OVERRIDE	1

/* Program name for error messages */
char *		progname = "AX";

/* Command line options */
int		cflag;
int		oneflag;
int		nflag;
int		stacksize;
capability *	pool;
capability *	owner;
capability *	prog;
int		empty_env;	/* Do not copy any of UNIX env to the Amoeba
				 * env.  Use only the -E settings.
				 */
char		*host;		/* Name given in -m option */
char 		*dumpfile;	/* Name given in -D option */

#ifdef UNIX
/* Capabilities for file server */
#include "ax.h"
static port		fsprivport;
static struct tty_caps	ax_tty[4];
static int		num_ax_ttys;
#endif

capability	process_cap;

#ifndef UNIX
capability my_proc_cap, my_owner_cap;
#endif

/* Forward declarations */
void		addcap();
void		addenv();
void		cap2list();
void		usage();
char *		am_getenv();
void		am_putenv();
static void	setdefaults();

#ifdef UNIX
/* Externals */
extern void	fserver();
#endif


/*
**  Get an (unsigned) numeric parameter; return -1 if it is not numeric.
**  No overflow check.
*/
static int
getnum(s)
char *	s;
{
    int value = 0;
    
    for (; *s != '\0'; ++s) {
	if (!isdigit(*s))
	    return -1;
	value = value*10 + (*s - '0');
    }
    return value;
}


/*
** Main program.
** Parse command line options, start the process, handle errors.
** Start the file server and wait for its completion.
*/
main(argc, argv)
int	argc;
char **	argv;
{
    errstat	err;
    int		opt;
    struct caplist *ExecCapList;
    
#if defined(SYSV) || defined(AMOEBA) || defined(__STDC__)
    (void) setvbuf(stderr, (char *)NULL, _IOLBF, BUFSIZ);
#else
#ifdef _IOLBF
    setlinebuf(stderr);
#endif /* _IOLBF */
#endif /* SYSV || AMOEBA || __STDC__ */
    
#ifdef AMOEBA
   {

	/* Initialize inherited file descriptors and remove them from the
	 * capability environment (allowing override of STDIN/OUT/ERR with -C).
	 */
	_ajax_fdinit();
   }
#endif

#ifdef UNIX
    signal(SIGAMOEBA, SIG_IGN);
#endif
    
    /* Set trans() locate timeout to 5 seconds */
    (void) timeout((interval) 5000);

    /* Assign `basename argv[0]` to progname, if it makes sense */
    if (argc > 0 && argv[0] != NULL) {
	char *	p;

	if ((p = strrchr(argv[0], '/')) == NULL)
	    p = argv[0];
	else
	    ++p;
	if (*p != '\0')
	    progname = p;
    }
    
    /* Process command line options */
    while ((opt = getopt(argc, argv, "nK1co:s:m:p:C:E:x:D:")) != EOF) {
	switch (opt) {

	case 'D':
	    dumpfile = optarg;
	    break;

	case 'o': /* -o owner: set process owner */
	    {
		static capability ownercap;

		if ((err = name_lookup(optarg, &ownercap)) != STD_OK) {
		    fprintf(stderr, "%s: -h %s: not found (%s)\n",
					progname, optarg, err_why(err));
		    exit(2);
		}
		owner = &ownercap;
		break;
	    }
	
	case 'c': /* -c: write cluster capability to stdout (rarely used) */
		if (isatty(fileno(stdout))) {
		    fprintf(stderr, "%s: -c: output MUST be redirected\n",
								  progname);
		    exit(2);
		}
		cflag = 1;
		break;
	
	case 'n': /* -n: don't start file server */
		nflag = 1;
		break;
	
	case '1': /* -1: start only a single file server */
		oneflag = 1;
		break;
	
	case 's': /* -s number: stack size in kbytes */
		stacksize = getnum(optarg);
		if (stacksize < 0) {
		    fprintf(stderr, "%s: -s requires numeric parameter\n",
							    progname);
		    exit(2);
		}
		stacksize *= 1024;
		break;
	
	case 'm': /* -m machine: processor name */
	    {
		static capability	hostcap;
		static capability	poolcap;

		if ((err = host_lookup(optarg, &hostcap)) != STD_OK) {
		    fprintf(stderr, "%s: -m %s: host not found (%s)\n",
					progname, optarg, err_why(err));
		    exit(2);
		}
		if (dir_lookup(&hostcap, PROCESS_SVR_NAME, &poolcap) !=
								STD_OK) {
		    fprintf(stderr, "%s: -m %s: can't find %s/%s\n",
				progname, optarg, optarg, PROCESS_SVR_NAME);
		    exit(2);
		}
		host = optarg;
		cap2list(HOST_CAPENV, &hostcap, OVERRIDE);
		pool = &poolcap;
		break;
	    }
	
	case 'p': /* -p procsvr: set process server */
		  /* E.g., -p /public/hosts/mark/proc */
	    {
		static capability	procsvrcap;

		if ((err = name_lookup(optarg, &procsvrcap)) != STD_OK) {
		    fprintf(stderr, "%s: -p %s: not found (%s)\n",
					progname, optarg, err_why(err));
		    exit(2);
		}
		pool = &procsvrcap;
		break;
	    }
	
	case 'C': /* -C name=capname: add capability to environment */
		addcap(optarg);
		break;
	
	case 'E': /* -E name=value: add string to shell environment */
		if (optarg[0] == '0' && !optarg[1])
		    empty_env++;
		else
		    addenv(optarg);
		break;
	
	case 'x': /* -x progfile: program file (overrides argv[0]) */
		/* Special case: '-' means read cap from stdin */
		if (strcmp(optarg, "-") == 0) {
		    /* Read process descriptor capability from stdin */
		    if (isatty(fileno(stdin))) {
			fprintf(stderr, "%s: -x -: input MUST be redirected\n",
								progname);
			exit(2);
		    }
		    if (fread((char*) &process_cap, CAPSIZE, 1, stdin) != 1) {
			fprintf(stderr, "%s: no capability read from stdin\n",
								progname);
			exit(1);
		    }
		    if (!nflag) {
			if (freopen("/dev/tty", "r", stdin) == NULL)
			    fprintf(stderr,
		"%s: warning: can't redirect input to /dev/tty\n", progname);
		    }
		}
		else if ((err = name_lookup(optarg, &process_cap)) != STD_OK) {
			fprintf(stderr, "%s: can't find %s (%s)\n",
					progname, optarg, err_why(err));
			exit(1);
		}
		prog = &process_cap;
		break;
	
	default: /* Unrecognized option */
		usage();
		/*NOTREACHED*/
	
	}
    }
    
    if (optind >= argc) {
	/* No arguments at all */
	usage();
	/*NOTREACHED*/
    }

    /* Set $HOME to a more useful default */
    if (am_getenv(HOME_ENV) == NULL) {
	char *home_env =
	  (char *) malloc((unsigned) (strlen(HOME_ENV) + strlen(HOME_DIR) + 2));
	sprintf(home_env, "%s=%s", HOME_ENV, HOME_DIR);
	addenv(home_env);
    }
    /* Set $SHELL to default */
    if (am_getenv("SHELL") == NULL) {
	static char shell_env[] = "SHELL=/bin/sh";

	addenv(shell_env);
    }
    /* Make name of host available to Amoeba program: */
    if (host) {
	char *host_env =
	      (char *) malloc((unsigned) (strlen(HOST_ENV) + strlen(host) + 2));
	sprintf(host_env, "%s=%s", HOST_ENV, host);
	addenv(host_env);
    }
    
    /* If command was given as "-" and -x flag not used to specify the program,
       user wants processor descriptor capability to be read from stdin: */
    
    if (strcmp(argv[optind], "-") == 0 && prog == NULL) {
	/* Read process descriptor capability from standard input */
	if (isatty(fileno(stdin))) {
	    fprintf(stderr, "%s: -: input MUST be redirected\n", progname);
	    exit(2);
	}
	if (fread((char*) &process_cap, CAPSIZE, 1, stdin) != 1) {
	    fprintf(stderr, "%s: no capability read from stdin\n", progname);
	    exit(1);
	}
	if (freopen("/dev/tty", "r", stdin) == NULL)
	    fprintf(stderr,
		  "%s: warning: can't redirect input to /dev/tty\n", progname);
	prog = &process_cap;
    }
    
    setdefaults(); /* Fill in any missing defaults */
    
#ifdef AMOEBA
    /* Copy the current capability environment with _ajax_makecapv()
     * in order to pass open (possibly redirected) file descriptors
     * unmodified to the child.
     */
    {
	static char namebuf[1000];
	static int fdlist[] = { 0, 1, 2 };
	static struct caplist NewCapList[NCAP + 4];
        extern struct caplist *capv;
        struct caplist *savecapv;

	/* Set the global capv temporarily to the new capabilities.
	 * It is used by _ajax_makecapv() to build the new capvec.
	 */
	savecapv = capv;
	capv = &CapList[0];
	_ajax_makecapv(3, fdlist, &NewCapList[0], NCAP + 4,
		       namebuf, (int)sizeof(namebuf));
	capv = savecapv;
	ExecCapList = NewCapList;
    }
#else
    ExecCapList = CapList;
#endif

    /* Start the process */
    err = exec_file(prog, pool, owner, stacksize, argv + optind,
			    am_environ, ExecCapList, &process_cap);
    if (err != STD_OK) {
	fprintf(stderr, "%s: can't execute '%s' (%s)\n",
					progname, argv[optind], err_why(err));
	exit(1);
	/*NOTREACHED*/
    }
    if (cflag) {
	/* Write process capability to standard output */
	(void) fwrite((char*) &process_cap, CAPSIZE, 1, stdout);

	/* Redirect stdout to stderr: */
	(void) close(1);
	(void) dup(2);
    }
#ifdef UNIX
    if (!nflag) {
	/* Start fileserver */
	fserver(oneflag, &fsprivport, ax_tty, num_ax_ttys);
	/* Doesn't return */
    }
#else
    /* Tell my owner to change its notion of child process from me to the
     * process I've just invoked, to get the effect of UNIX exec (the other
     * part of getting this effect was that the owner of the new process
     * got set to my owner, in setdefaults:
     */
    err = pro_swapproc(&my_owner_cap, &my_proc_cap, &process_cap);
    if (err != STD_OK && err != STD_CAPBAD && err != STD_ARGBAD) {
	/* Don't complain about STD_CAPBAD:  this can happen when the child
	 * exits very quickly.  The owner then thinks that *we* just exited,
	 * but that's exactly what we want.
	 * Don't complain about STD_ARGBAD either: if the child quickly
	 * announces itself to the session server by means of "ses_init",
	 * then pro_swapproc will be refused as well.
	 */
	fprintf(stderr, "%s: pro_swapproc failed (%s)\n",
				    progname, err_why(err));
	exit(1);
    }
    /* Set my owner cap to NULL, so that my owner is not notified when I
     * exit:
     */
    {	static capability nullcap;
	if ((err = pro_setowner(&my_proc_cap, &nullcap)) != STD_OK) {
	    fprintf(stderr, "%s: pro_setowner failed (%s)\n",
					progname, err_why(err));
	    exit(1);
	}
    }
#endif
    return 0;
}


/*
** Print elaborate usage message.
*/
void
usage()
{
    fprintf(stderr,
	    "usage: %s [options] progname [argument] ...\n", progname);
    fprintf(stderr, "-n:          no I/O server\n");
    fprintf(stderr, "-1:          1 I/O server only\n");
    fprintf(stderr, "-c:          capability of process to stdout\n");
    fprintf(stderr, "-o owner:    owner capability\n");
    fprintf(stderr, "-s number:   stack size in Kbytes\n");
    fprintf(stderr, "-m machine:  machine name or pathname\n");
    fprintf(stderr, "-D dumpfile: core dump file\n");
    fprintf(stderr,
	    "-p procsvr:  process server (e.g., -p /public/hosts/foo/proc)\n");
    fprintf(stderr, "-C name=cap: add to capability environment\n");
    fprintf(stderr,
		    "-E 0:        initialize environment to empty\n");
    fprintf(stderr, "-E name=str: add to string environment\n");
    fprintf(stderr, "-x progfile: executable file (default uses progname)\n");
    fprintf(stderr, "             ('-' to read capability from stdin)\n");
    fprintf(stderr, "progname:    program name\n");
    exit(2);
}


/*
** Set default parameters. Called *after* option processing.
*/
static void
setdefaults()
{
#ifdef UNIX
    static capability cap;
    errstat err;
    
    if ((err = name_lookup("/", &cap)) != STD_OK) {
	fprintf(stderr, "%s: can't find / capability (%s)\n",
						progname, err_why(err));
	exit(1);
    }
    cap2list("ROOT", &cap, NO_OVERRIDE);
    cap2list("WORK", &cap, NO_OVERRIDE);
    
    if ((err = name_lookup(DEF_SESSION_CAP, &cap)) == STD_OK)
	cap2list(SESSION_CAPENV, &cap, NO_OVERRIDE);
    
    /* Make all the caps for the tty devices */
    if (!nflag) {
    	int tty;
	port random;

	uniqport(&fsprivport);

	/* Set up stdin cap */
	ax_tty[0].t_fd = fileno(stdin);
	priv2pub(&fsprivport, &ax_tty[0].t_cap.cap_port);
	uniqport(&random);
	prv_encode(&ax_tty[0].t_cap.cap_priv, (objnum) 0,
					    PRV_ALL_RIGHTS, &random);

	/* Set up stdout cap */
	ax_tty[1].t_fd = fileno(stdout);
	ax_tty[1].t_cap.cap_port = ax_tty[0].t_cap.cap_port;
	uniqport(&random);
	prv_encode(&ax_tty[1].t_cap.cap_priv, (objnum) 1,
					    PRV_ALL_RIGHTS, &random);

	/* Set up stderr cap */
	ax_tty[2].t_fd = fileno(stderr);
	ax_tty[2].t_cap.cap_port = ax_tty[0].t_cap.cap_port;
	uniqport(&random);
	prv_encode(&ax_tty[2].t_cap.cap_priv, (objnum) 2,
					    PRV_ALL_RIGHTS, &random);

	cap2list("STDIN",  &ax_tty[0].t_cap, NO_OVERRIDE);
	cap2list("STDOUT", &ax_tty[1].t_cap, NO_OVERRIDE);
	cap2list("STDERR", &ax_tty[2].t_cap, NO_OVERRIDE);
	num_ax_ttys = 3;

	tty = open("/dev/tty", 2);
	if (tty >= 0) {
		ax_tty[3].t_fd = tty;
		ax_tty[3].t_cap.cap_port = ax_tty[0].t_cap.cap_port;
		uniqport(&random);
		prv_encode(&ax_tty[3].t_cap.cap_priv, (objnum) 3,
						PRV_ALL_RIGHTS, &random);
		cap2list("TTY", &ax_tty[3].t_cap, NO_OVERRIDE);
		num_ax_ttys++;
	}
	if (owner == NULL)
	    owner = &ax_tty[0].t_cap;
    }
#else
    extern struct caplist *capv;
    struct caplist *c;
    errstat err;

    /* Copy all caps from my environment into child environment: */
    for (c = capv ; c->cl_name != NULL; ++c)
	cap2list(c->cl_name, c->cl_cap, NO_OVERRIDE);

    if (getinfo(&my_proc_cap, (process_d *)0, 0) < 0 ||
				    NULLPORT(&my_proc_cap.cap_port)) {
	fprintf(stderr, "%s: getinfo error\n", progname);
	exit(1);
    }
    if ((err = pro_getowner(&my_proc_cap, &my_owner_cap)) != STD_OK) {
	fprintf(stderr, "%s: pro_getowner failed (%s)\n",
				    progname, err_why(err));
	exit(1);
    }
    /* Unless user specified explicit owner cap, tell the new process
     * that my owner is its owner, so I can exit after startup.  See
     * also the call to pro_swapproc, just before exit:
     */
    if (owner == NULL)
	owner = &my_owner_cap;
#endif
}


/*
**  Process -C name=capname argument. Check format, then add cap.
*/
void
addcap(arg)
char *	arg;
{
    char *	name = arg;	/* pointer to 'name' part of string */
    char *	capname;	/* pointer to 'capname' part of string */
    capability	cap;
    errstat	err;
    
    if ((capname = strchr(arg, '=')) == NULL) {
	fprintf(stderr, "%s: -C argument should be name=cap\n", progname);
	exit(2);
    }
    
    /* Add null terminator to 'name' and move to start of capability name */
    /* NOTE: The following is modifying argv for the external world to see.
     * This happens to be a good thing under Sun OS, because ps fails when
     * an arg is of the form VAR=value:
     */
    *capname++ = '\0';
    
    if ((err = name_lookup(capname, &cap)) != STD_OK) {
	fprintf(stderr, "%s: -C %s=%s: can't find %s (%s)\n",
			    progname, name, capname, capname, err_why(err));
	exit(1);
    }
    
    cap2list(name, &cap, OVERRIDE);
}


/*
** Like the UNIX version, except works on am_environ, instead of environ:
*/
char *
am_getenv(name)
char *	name;
{
        char *env_lookup();

        return env_lookup(am_environ, name);
}

/*
** Like the UNIX version, except works on am_environ, instead of environ:
*/
void
am_putenv(arg)
char *	arg;
{
        char **new_env;
        if ((new_env = env_put(am_environ, &am_env_alloc, arg, 1)) != NULL)
                am_environ = new_env;
}



/*
** Process -E name=string argument. Check format, then add string.
*/
void
addenv(arg)
char *	arg;
{
    char *	value;

    if ((value = strchr(arg, '=')) == NULL) {
	fprintf(stderr, "%s: -E argument should be name=value\n", progname);
	exit(2);
    }

    /* Work around for bug in Sun OS ps command.  It will not show the
     * arguments of any program that has an arg of the form VAR=value, so
     * we make a copy of the arg and then modify the '=' in the original to
     * a ':'.  Sorry about that, VAX people.
     */
    arg = strcpy((char *) malloc((unsigned) strlen(arg) + 1), arg);
    *value = ':';

    /* Initialize Amoeba environ to a copy of UNIX, if desired: */
    if (!empty_env && am_environ[0] == NULL) {
	char **ep;
	/* Copy UNIX env to Amoeba env: */
	for (ep = environ; *ep; ep++)
	    am_putenv(*ep);
    }
    am_putenv(arg);
}


/*
** Add capability to list, perhaps overriding earlier value.
*/
void
cap2list(name, cap, override)
char *name;
capability *cap;
int override;
{
    struct caplist *c = CapList;
    
    /* See if 'name' is already in the list */
    for ( ; c->cl_name != NULL; ++c) {
	if (strcmp(c->cl_name, name) == 0) {
	    if (override)
		*c->cl_cap = *cap;
	    return;
	}
    }
    
    /* Add new item to list */
    if (c >= &CapList[NCAP]) {
	fprintf(stderr, "%s: too many environment capabilities\n", progname);
	exit(1);
    }
    c->cl_name = name;
    c->cl_cap = (capability *) malloc(sizeof (capability));
    if (c->cl_cap == NULL) {
	fprintf(stderr, "%s: not enough memory for capability environment\n",
								    progname);
	exit(1);
    }
    *c->cl_cap = *cap;
}

