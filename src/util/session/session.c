/*	@(#)session.c	1.8	96/02/27 13:14:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Session server main program -- organized strictly bottom-up */

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <amtools.h>
#include <ampolicy.h>
#include <caplist.h>
#include <class/sessvr.h>
#include <module/rnd.h>
#include <module/proc.h>
#include <module/signals.h>
#include <capset.h>
#include <soap/soap.h>
#include "alarm.h"
#include "devnullsvr.h"
#include "devproc.h"
#include "session.h"

/* External declarations */
extern void init_sesimpl();
extern struct caplist *capv;		/* Capability environment */

#define NTHREAD		4		/* Number of session server threads */
#define SES_STACKSIZE	16000		/* OK since trans buffer is malloced */

/* Local flags */
static int force = 0;			/* -f sets */

/* Global flags */
int attach = 0;				/* -a sets */
int batchjob = 0;			/* -b sets */
int debugging = 0;			/* -d sets */
int permanent = 0;			/* -a or no command sets */

/* Global parameters */
char *progname = "SESSION";		/* basename(argv[0]) sets */
char *capname = NULL;			/* -c or default processing sets */

/* Other file-global variables */
static int oldlives = 0;		/* Set if another server lives */
static int published = 0;		/* Set if capability published */
static port privport;			/* Server's private port */
capability Session_pubcap;		/* Server's published capability */
static capability oldcap;		/* Existing server's capability */
port Ses_checkfield;			/* Checkfield for session cap */
static char oldinfo[100];		/* Existing server's std_info  */

extern void whoami();
extern errstat ml_sessvr();

/* Print elaborate usage message and exit(2) */

static void
usage(msg)
char *msg;
{
	fprintf(stderr, "%s: %s\n", progname, msg);
	fprintf(stderr,
		"usage: %s [-a] [-b] [-c capname] [-d] [-f] [command [arg] ...]\n",
		progname);
	fprintf(stderr,
		"-a: attach to existing session svr if possible\n");
	fprintf(stderr,
		"-b: batch job; don't add '-' prefix to command\n");
	fprintf(stderr,
		"-c capname: session svr capability [%s if published]\n",
		DEF_SESSION_CAP);
	fprintf(stderr, "-d: debugging output\n");
	fprintf(stderr, "-f: force overriding existing capname\n");
	fprintf(stderr, "command [arg] ...: command to run\n");
	exit(2);
	/*NOTREACHED*/
}


errstat
publish_devconsolecap(conscap)
capability *	conscap;
{
	errstat		err;
	long		cols[3];
	capset		cs;

	cols[0] = 0xFF;
	cols[1] = 0xFF;
	cols[2] = 0;
	if (conscap == (capability *) 0) {
		warning("has no STDOUT capability");
		return STD_SYSERR;
	}
	(void) name_delete(DEF_SESSION_CONSOLE);

	if (!cs_singleton(&cs, conscap)) {
		fatal("cannot allocate session capset");
	}
	err = sp_append(SP_DEFAULT, DEF_SESSION_CONSOLE, &cs, 3, cols);
	if (err != STD_OK) {
		warning("cannot store console cap %s (%s)",
			DEF_SESSION_CONSOLE, err_why(err));
	}
	return err;
}


errstat
republish_devconsolecap(conscap)
capability *	conscap;
{
	errstat		err;
	long		cols[3];
	capset		cs;

	if (conscap == (capability *) 0) {
		warning("has no STDOUT capability");
		return STD_SYSERR;
	}

	if (!cs_singleton(&cs, conscap)) {
		fatal("cannot allocate session capset");
	}
	err = sp_replace(SP_DEFAULT, DEF_SESSION_CONSOLE, &cs);
	if (err != STD_OK)
	    err = sp_append(SP_DEFAULT, DEF_SESSION_CONSOLE, &cs, 3, cols);
	if (err != STD_OK) {
		warning("cannot store console cap %s (%s)",
			DEF_SESSION_CONSOLE, err_why(err));
	}
	return err;
}


errstat
unpublish_devconsolecap()
{
	errstat err;
	
	if ((err = name_delete(DEF_SESSION_CONSOLE)) != STD_OK)
		warning("cannot delete console server cap %s (%s)",
			DEF_SESSION_CONSOLE, err_why(err));
	return err;
}


/* Exit with given status.  First delete capabilities we installed. */

void
goaway(sts)
	int sts;
{
	if (published) {
		if (capname != NULL)
			(void) name_delete(capname);
		(void) unpublish_devnullcap();
		(void) unpublish_devproccap();
		(void) unpublish_devconsolecap();
	}
	exit(sts);
}

/*
 * varargs diagnostics routines
 */

#ifdef __STDC__
#define va_dostart(ap, format)	va_start(ap, format)
#else
#define va_dostart(ap, format)	va_start(ap)
#endif

#ifdef __STDC__
void fatal(char *format, ...)
#else
/*VARARGS1*/
void fatal(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "session: fatal: ");
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    goaway(1);
}

#ifdef __STDC__
void warning(char *format, ...)
#else
/*VARARGS1*/
void warning(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "session: warning: ");
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/* Check whether capname specifies a living server.
   The resulting status is stored in oldlives.
   Its capability is filled in oldcap and its info string in oldinfo.
   If the name exists but no server answers, it is deleted.
   Do not call when capname is NULL. */

static void
check_oldsvr()
{
	errstat err;
	interval savetout;
	int n;
	
	if (debugging)
		fprintf(stderr, "%s: checking old session svr %s ...\n",
							progname, capname);
	err = name_lookup(capname, &oldcap);
	if (err != STD_OK) {
		if (err != STD_NOTFOUND)
			warning("old session server not found (%s)",
				err_why(err));
		else if (debugging)
			fprintf(stderr, "%s: old session svr %s not found\n",
							progname, capname);
		return;
	}
	savetout = timeout((interval) 2000);
	err = std_info(&oldcap, oldinfo, sizeof oldinfo - 1, &n);
	(void) timeout(savetout);
	if (err != STD_OK) {
		warning("old session server does not respond (%s)",
			err_why(err));
		err = name_delete(capname);
		if (err != STD_OK)
		    warning("cannot delete old session svr capability (%s)",
			    err_why(err));
	}
	else {
		oldinfo[n] = '\0';
		if (debugging)
			fprintf(stderr, "%s: %s lives: %s\n",
						progname, capname, oldinfo);
		oldlives = 1;
	}
}


/* Signal catcher for transaction signal in session server threads */

/*ARGSUSED*/
static void
transsig_catcher(sig, us, extra)
signum sig;
thread_ustate *us;
_VOIDSTAR extra;
{
	_ajax_puts("session svr: catching transaction signal");
}


/* Call the session server main loop */

/*ARGSUSED*/
static void
call_ml_sessvr(p, s)
char * p;
int s;
{
	errstat err;
	
	/* XXX experiment: catch transaction signal (signal 0) */
	sig_catch((signum) 0, transsig_catcher, (_VOIDSTAR) 0);
	for (;;) {
		err = ml_sessvr(&privport);
		if (err != RPC_ABORTED)
			break;
	}
	/* maybe the transaction buffer could not be allocated? */
	warning("main loop returned unexpectedly (%s)", err_why(err));
	/* NOTREACHED */
}


/* Give up trying to add new server threads when it has failed
 * MAX_NEWTHREAD_FAILED times in a row.
 */
#define MAX_NEWTHREAD_FAILED	5

void
check_svr_threads()
{
    extern int nprocs_busy;
    static int nstarted = 0;
    static int failed = 0;
    int i, wanted;

    /* In case we have a lot of background processes, add more threads: */
    wanted = NTHREAD + nprocs_busy / 4;

    for (i = nstarted; i < wanted && failed < MAX_NEWTHREAD_FAILED; i++) {
	if (debugging) {
	    fprintf(stderr, "%s: starting session svr thread %d ...\n",
		    progname, i);
	}

	if (!thread_newthread(call_ml_sessvr, SES_STACKSIZE, (char *)0, 0)) {
	    if (failed == 0) {
		/* only warn once */
		warning("cannot start new session svr thread");
	    }
	    failed++;

	    /* probably failed because of (temporary) lack of memory */
	    break;
	} else {
	    failed = 0;
	    nstarted++;
	}
    }

    if (nstarted == 0) {
	fatal("could not start any session server threads");
    }
}

/* Start the session server threads, the sweeper and the /dev/null server */

static void
start_svrs()
{
	uniqport(&privport);
	priv2pub(&privport, &Session_pubcap.cap_port);
	uniqport(&Ses_checkfield);
	if (prv_encode(&Session_pubcap.cap_priv, (objnum) 0,
		       PRV_ALL_RIGHTS, &Ses_checkfield) != 0)
	{
	    fatal("prv_encode for supercap failed");
	}

	(void) start_devprocsvr();
	init_sesimpl();
	check_svr_threads();	/* initially create NTHREAD server threads */
	start_sweeper();
	start_alrmsvr();
	(void) start_devnullsvr();
}


/* Publish the session server capability as capname.
   Also publish the /dev/null server capability.
   Do not call when capname is NULL. */

static rights_bits sescap_cols[] = { PSR_ALL, PSR_READ, PSR_READ };

static void
publish_caps()
{
	capset pub_cs;
	errstat err;
	
	if (debugging)
		fprintf(stderr,
			"%s: publish session svr capability as %s ...\n",
			progname, capname);
	/* XXX This solution has a race condition.
	   We should be more careful: when another session server is
	   also starting up (with -a), we want exactly one server to
	   live and the other to go away (running its command with
	   the former). */
	if (!cs_singleton(&pub_cs, &Session_pubcap)) {
	    fatal("cannot allocate session capset");
	}
	err = sp_append(SP_DEFAULT, capname, &pub_cs, 3, sescap_cols);
	if (err != STD_OK) {
	    fatal("cannot publish original session svr capability (%s)",
		  err_why(err));
	}

	if (debugging)
		fprintf(stderr, "%s: publish /dev/null ...\n", progname);
	(void) publish_devnullcap();
	(void) publish_devproccap();
	(void) publish_devconsolecap(getcap("STDOUT"));
	published = 1;
}


/* Build capability list for startcommand() */

static struct caplist *
build_caplist()
{
	struct caplist *newcapv;
	int ncaps;
	int i, j;
	
	/* Count capabilities in current caplist */
	ncaps = 2; /* Reserve one slot for "_SESSION", one for sentinel */
	for (i = 0; capv[i].cl_name != NULL; ++i) {
		if (capv[i].cl_name[0] != '_')
			++ncaps;
	}
	
	/* Allocate new list */
	newcapv = (struct caplist *)
			 malloc((size_t) (ncaps * sizeof(struct caplist)));
	if (newcapv == NULL)
		fatal("no memory for new capability environment for command");
	
	/* Put session cap in new list */
	newcapv[0].cl_name = "_SESSION";
	newcapv[0].cl_cap = &Session_pubcap;
	j = 1;
	
	/* Add old caps to new list */
	for (i = 0; capv[i].cl_name != NULL; ++i) {
		if (capv[i].cl_name[0] != '_') {
			newcapv[j] = capv[i];
			++j;
		}
	}
	
	newcapv[j] = capv[i]; /* Copy sentinel (all NULLs) */
	
	return newcapv;
}


/* Execute the command */

static void
exec_command(argv)
	char **argv;
{
	capability progcap;
	capability process;
	capability self;
	capability owner;
	char *file, *p;
	char buf[256];
	errstat err;
	
	file = argv[0];
	err = name_lookup(file, &progcap);
	if (err != STD_OK) {
		fatal("cannot find program %s (%s)\n", file, err_why(err));
	}
	
	if (attach) {
		if (getinfo(&self, (process_d *)NULL, 0) < 0)
			fatal("cannot get my own process capability");
		if ((err = pro_getowner(&self, &owner)) != STD_OK)
			fatal("cannot get my own owner (%s)", err_why(err));
	}
	else {
		owner = Session_pubcap;
	}
	
	if (!batchjob) {
		/* Prefix argv[0] with '-' so shell will read .profile */
		p = strrchr(file, '/');
		if (p == NULL)
			p = file;
		else
			p++;
		buf[0] = '-';
		strcpy(buf+1, p);
		argv[0] = buf;
	}
	
	if (debugging)
		fprintf(stderr, "%s: starting %s...\n", progname, file);
	err = exec_file(
		/*prog*/	&progcap,
		/*host*/	NILCAP,
		/*owner*/	&owner,
		/*stacksize*/	0,
		/*argv*/	argv,
		/*envp*/	(char **)NULL,
		/*caps*/	build_caplist(),
		/*process_ret*/	&process
		);
	if (err != STD_OK) {
		fatal("cannot start %s (%s)\n", file, err_why(err));
	}
	if (debugging)
		fprintf(stderr, "%s: %s started\n", progname, file);
	if (attach) {
		static capability nullcap; /* Constant */
		if ((err = pro_setowner(&self, &nullcap)) != STD_OK)
		    warning("cannot set my own owner (%s)", err_why(err));
		if ((err = pro_swapproc(&owner, &self, &process)) != STD_OK)
		    warning("cannot swap process (%s)", err_why(err));
	}
}


main(argc, argv)
	int argc;
	char **argv;
{
	(void) setvbuf(stderr, (char *)NULL, _IOLBF, (size_t)BUFSIZ);

	(void) timeout(10000L); /* Force non-infinite default timeout */
	
	if (argc > 0 && argv[0] != NULL) {
		char *p = strrchr(argv[0], '/');
		if (p == NULL)
			p = argv[0];
		else
			++p;
		if (*p != '\0')
			progname = p;
	}
	
	for (;;) {
		int opt = getopt(argc, argv, "abc:dfp");
		if (opt == EOF)
			break;
		switch (opt) {
		case 'a':
			attach = 1;
			break;
		case 'b':
			batchjob = 1;
			break;
		case 'p':
			permanent = 1;
			break;
		case 'c':
			capname = optarg;
			break;
		case 'f':
			force = 1;
			break;
		case 'd':
			debugging = 1;
			break;
		default:
			usage("unrecognized option");
			/*NOTREACHED*/
		}
	}
	
	if (attach && force)
		usage("-a and -f together don't make sense");
#if 0
	if (optind >= argc)
		permanent = 1;
#endif
	if ((attach || permanent) && (capname == NULL))
		capname = DEF_SESSION_CAP;
	
	/* First initialize the builtin directory server */
	init_dirsvr();

	whoami();

	if (capname != NULL) {
		check_oldsvr();
		if (oldlives) {
			if (attach) {
				Session_pubcap = oldcap;
				if (optind < argc)
					exec_command(&argv[optind]);
				exit(0);
			}
			else {
				if (!force)
				    fatal("will not replace existing server");
				else {
				    errstat err;

				    err = name_delete(capname);
				    if (err != STD_OK)
					fatal("-f: cannot delete cap (%s)",
					      err_why(err));
				    oldlives = 0;
				}
			}
		}
	}
	start_svrs();
	if (capname != NULL)
		publish_caps();
	if (optind < argc)
		exec_command(&argv[optind]);
	thread_exit();
	/* NOTREACHED */
}
