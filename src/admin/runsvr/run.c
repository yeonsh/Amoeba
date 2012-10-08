/*	@(#)run.c	1.8	96/02/27 10:18:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*

RUN SERVER
----------

This server attempts load balancing of processor pools by directing new
jobs to the most appropriate processor, based on available resources and
the job's resource requirements.  Of course, this is often a wild
guess, since it can't predict what a job is going to do, and it can't
predict what resources will become available soon.  Hence, there should
be a second form of load balancing, based on migration of jobs to
processors where they will run better.  This is not the purpose of the
run server, however.

The server manages an arbitrary number of processor pools (*pooldirs*),
each with subdirectories for multiple architectures (*archdirs*).
Hosts may be members of more than one archdir.  There is a capability
for each pooldir.  Requests directed to a particular pooldir will only
choose from the hosts that are a member of a suitable archdir within
that pooldir.

The basic interface supported by the current run server is pd_multi_findhost.
The runserver is given a *set* of process descriptors (one per architecture),
and it has to return the capability (and name) of a processor in the pool
that's currently seems best suited to execute a (matching) process descriptor.

Much of the run server's code is devoted to keeping a fairly recent
idea of each pool processor's load, without using too much of the
system's resources, but also without long news black-outs when a
processor goes down suddenly.  The run server itself is currently not
replicated, but it is easy to ensure that a copy is up and running at all
times using the boot server.

There are three basic groups of threads: one group serves incoming
requests, one group regularly polls processors for their load, and one
group (running much less frequently) polls processors that are down to
see when they become available again.  The latter two groups of threads
are actually managed by the "tj" package, which manages queues of
function calls to be executed at an arbitrary time in the future.

The distinction between polling machines that are (supposed to be) up
and machines that are down is necessary, to avoid situations where all
available threads are blocked waiting for down processors (which may
take fairly long delays before the kernel decides that a remote
processor really doesn't respond).  In a large system, it isn't
sensible to devote a thread to each processor in the pool:  each thread
would cost a couple of Kbytes for stack space, plus a kernel data
structure for the thread's registers, etc.; and it would be idle most of
the time.  In contrast, the "tj" package uses only a 16-byte structure
to store information about a scheduled job.

There are multiple server threads to serve multiple clients in
parallel:  While handling a request for one pool dir,
clients using a different pool dir should not have to wait.
Also, when requests are coming in rapid succession from a
single client, the next request can arrive before the serving thread has
executed the next getreq call, in which the client's kernel would
attempt a broadcast to locate the server.

Author: Guido van Rossum, CWI, June, 1989
Modified: Kees Verstoep, VU, June, 1991

Change log:

May 1990:	restructured to manage multiple pools and architectures;
		added std_status request; changed evaluation formula and
		added control requests to change the parameters; added
		request to rescan an archdir.

June 1991:	- made heterogenous process startup possible
		- made the run server "object oriented";  it is now possible
		  (even for "humble users") to add pooldirs without having
		  to restart the run server.
		- now uses the std_getparams/std_setparams interface
		- std_touch is now used to cause a rescan
		- added rights checking on all commands
		- added locking of pooldir objects while doing operations
		- removed pro_getdef/pro_exec support (it does not work
		  work for heterogeneous pools, and nobody seems to use
		  this feature anyway).  If at a later stage we do want the
    		  run server to be able to start up processes for us, the
		  interface has to become different.
*/


#include <stdlib.h>
#include <string.h>
#define _POSIX_SOURCE
#include <limits.h>

#include "runsvr.h"
#include "ampolicy.h"
#include "module/rnd.h"
#include "module/buffers.h"
#include "tj.h"
#include "segcache.h"
#include "runrights.h"
#include "capset.h"
#include "server/soap/soap.h"
#include "objects.h"
#include "thread.h"

extern unsigned sleep();

/* Miscellaneous parameterizations */

#define SEC		((interval)1000)/* Delay of one second */
#define IMMEDIATELY	((interval)0)	/* No delay */
#define SERVERSTACK	8000		/* Stack size for server threads.
					 * Should be enough since the trans
					 * buffer will be malloced.
					 */
#define JOBQUEUESTACK	8000L		/* Stack size for job queues */


/* Initialization parameters and their defaults */

static char *admin_dir = DEF_RUN_ADMIN;
static char *publish_dir = PUB_RUNSVR_DIR;
static char *run_name = "default";
static char *publish_pools_dir = "pools";

int nservers = 4;			/* Number of server threads */
int nupthreads = 2;			/* Number of up job queue threads */
int ndownthreads = 2;			/* Number of down job queue threads */

/*
 * NOTE that the pollupdelay should not be less than 5 seconds.
 * Otherwise the process server will change its exec caps too fast
 * and process creation becomes too unreliable
 */
interval pollupdelay = 5*SEC;		/* Poll frequency for up hosts */
interval pollcrashdelay	= 10*SEC;	/* Poll delay right after crash */
interval polldowndelay = 60*SEC;	/* Poll frequency for down hosts */
interval pollstartdelay = SEC/2;	/* Poll delay after starting process */

interval polluptimeout = 5*SEC;		/* Locate time-out for up hosts */
interval polldowntimeout = 2*SEC;	/* Locate time-out for down hosts */
#if 0
interval lookuptimeout = 5*SEC;		/* Locate time-out for lookups */
interval shorttimeout = 2*SEC;		/* Short locate time-out */
#endif
interval servertimeout = 10*SEC;	/* Locate time-out for svr threads */

/* Globals used in other files */

int debugging;				/* Debugging level */
char *progname = "RUN";			/* Program name (set in main) */
struct hostinfo *First_host;		/* List of all hostinfo structs */
struct tjobset *upjobs, *downjobs;	/* Separate job queues for up/down */

extern void procgcjob();		/* Job to kill orphans */

/*
 * forward declarations for static functions defined further on
 */
static char *prsince();
static void setupqueues();
static void callserver();
static void polldown();
static void pollup();
static void adduppoll();
static void polldown();
static void pollup();
static void startservers();

void
refresh_adminfile()
/* create and install a new file containing runsvr object administration */
{
    if (!obj_store(admin_dir, run_name)) {
	fatal("could not store administration file");
    }
}
	    
static void
publish_supercap(dir, name)
char *dir;
char *name;
{
    static long	super_colmask[] = {
	RUN_RGT_ALL,
	RUN_RGT_CREATE | RUN_RGT_STATUS,
	RUN_RGT_CREATE | RUN_RGT_STATUS
    };
    capset	dir_cs;
    capset	super_cs;
    capability	supercap;
    errstat	err;

    /* create super object */
    if ((err = new_super_obj(&supercap)) != STD_OK) {
	fatal("cannot create new super object (%s)", err_why(err));
    }
    if (cs_singleton(&super_cs, &supercap) != 1) {
	fatal("cannot create capset for new super object");
    }
    
    /* look up publishing dir */
    if ((err = sp_lookup(SP_DEFAULT, dir, &dir_cs)) != STD_OK) {
	fatal("cannot find publishing directory '%s' (%s)",
	      dir, err_why(err));
    }

    /* append it to the directory, with appropriate column mask */
    err = sp_append(&dir_cs, name, &super_cs,
		    sizeof(super_colmask)/sizeof(long), super_colmask);
    if (err != STD_OK) {
	fatal("cannot publish supercap in '%s' (%s)", dir, err_why(err));
    }

    cs_free(&super_cs);
    cs_free(&dir_cs);
}


static void
make_publish_pooldir()
{
    errstat	err;
    char	dir[PATH_MAX];
    capset	cs;

    sprintf(dir, "%s/%s", admin_dir, publish_pools_dir);
    err = sp_lookup(SP_DEFAULT, dir, &cs); 
    if (err == STD_OK) {
	return;
    }

    /* Let's make the required directory */
    err = sp_lookup(SP_DEFAULT, admin_dir, &cs); 
    if (err != STD_OK) {
	fatal("cannot find directory '%s' (%s)\n", admin_dir, err_why(err));
    }
    err = sp_mkdir(&cs, publish_pools_dir, (char **) NULL);
    if (err != STD_OK) {
	fatal("cannot make directory '%s' (%s)\n", dir, err_why(err));
    }
}


/* The run server saves the capabilities for the pool *directories*
 * in <admin_dir>/pools, so that the sysadmin can disable any or
 * all pools at need.
 */

void
publish_pooldir(dircap, obj_nr)
capability *	dircap;
objnum		obj_nr;
{
    errstat	err;
    capset	pools_dir;
    capset	dircap_cs;
    static long	colmasks[] = { RUN_RGT_ALL, 0, 0 };
    char	name[15];
    char	dir[PATH_MAX];

    sprintf(dir, "%s/%s", admin_dir, publish_pools_dir);
    err = sp_lookup(SP_DEFAULT, dir, &pools_dir); 
    if (err != STD_OK) {
	fatal("cannot find pools publishing directory '%s' (%s)\n",
							dir, err_why(err));
    }
    if (cs_singleton(&dircap_cs, dircap) != 1) {
	fatal("cannot create capset for pool object");
    }
    sprintf(name, "pool_%05d", obj_nr);
    err = sp_append(&pools_dir, name, &dircap_cs, 3, colmasks);
    if (err == STD_EXISTS) {
	/* Non fatal, since a temporary inconsitency between the statefile
	 * and the 'pools' directory might arise as a result of an untimely
	 * crash of the run server (or the host running it).
	 */
	error("pool cap %s/%s already exists; replacing it\n", dir, name);
	err = sp_replace(&pools_dir, name, &dircap_cs);
    }
    if (err != STD_OK) {
	fatal("cannot publish pool cap %s/%s (%s)\n", dir, name, err_why(err));
    }
}

void
unpublish_pooldir(obj_nr)
objnum		obj_nr;
{
    errstat	err;
    capset	pools_dir;
    char	name[15];
    char	dir[PATH_MAX];

    sprintf(dir, "%s/%s", admin_dir, publish_pools_dir);
    err = sp_lookup(SP_DEFAULT, dir, &pools_dir); 
    if (err != STD_OK) {
	fatal("cannot find pools publishing directory '%s' (%s)\n",
							dir, err_why(err));
    }
    sprintf(name, "pool_%05d", obj_nr);
    err = sp_delete(&pools_dir, name);
    if (err != STD_OK) {
	/* Non fatal: see remark in publish_pooldir above */
	error("cannot delete pool cap %s/%s (%s)\n", dir, name, err_why(err));
    }
}


static errstat
getsuper(dir, name, supercap)
char *dir;
char *name;
capability *supercap;
{
    errstat err;
    capset dir_cs;
    capset super_cs;

    if ((err = sp_lookup(SP_DEFAULT, dir, &dir_cs)) != STD_OK) {
	return err;
    }

    err = sp_lookup(&dir_cs, name, &super_cs);
    cs_free(&dir_cs);
    if (err != STD_OK) {
	return err;
    }

    err = cs_to_cap(&super_cs, supercap);
    cs_free(&super_cs);
    return err;
}

/* Print usage message and exit */

static void
usage()
{
	fprintf(stderr, "usage: %s [-options]\n\n", progname);
	fprintf(stderr, "-D number:\tnumber of 'down poll' threads [%d]\n",
								ndownthreads);
	fprintf(stderr, "-U number:\tnumber of 'up poll' threads [%d]\n",
								nupthreads);
	fprintf(stderr, "-S number:\tnumber of server threads [%d]\n",
								nservers);
	fprintf(stderr, "-a dir:\t\tadministration directory [\"%s\"]\n",
		admin_dir);
	fprintf(stderr, "-d:\t\tdebugging output (repeat to get more)\n");
	fprintf(stderr, "-i:\t\tinitialize super cap when not available\n");
#if 0
	fprintf(stderr, "-k:\t\tstun orphan processes with code -666\n");
#endif
	fprintf(stderr, "-n name:\trunsvr name [\"%s\"]\n", run_name);
	fprintf(stderr, "-p dir:\t\tsupercap publishing directory [\"%s\"]\n",
		publish_dir);
	
	exit(2);
}

/* Main program */

main(argc, argv)
	int argc;
	char **argv;
{
	capability supercap;
	int autoinit = 0;	/* If true, publish runsvr super cap */
	int killorphans = 0;	/* If true, kill orphan processes */
	int opt;
	errstat err;

	/* Assign `basename argv[0]` to progname */
	if (argc > 0 && argv[0] != NULL) {
		char *p = strrchr(argv[0], '/');
		if (p == NULL)
			p = argv[0];
		else
			++p;
		if (*p != '\0')
			progname = p;
	}
	
	/* Scan command line options */
	while ((opt = getopt(argc, argv, "D:U:S:a:dimn:p:s:")) != EOF) {
		switch (opt) {
		case 'D':	ndownthreads = atoi(optarg);	break;
		case 'U':	nupthreads = atoi(optarg);	break;
		case 'S':	nservers = atoi(optarg);	break;
		case 'd':	debugging++;			break;
		case 'a':	admin_dir = optarg;		break;
		case 'i':	autoinit++;			break;
#if 0
		case 'k':	killorphans++;			break;
#endif
		case 'n':	run_name = optarg;		break;
		case 'p':	publish_dir = optarg;		break;
		default:	usage();			break;
		}
	}

	if (optind < argc) {
	    /* caller probably used to the old `auto configuring' interface;
	       print a warning message */
	    error("ignored old-style pool arguments; use makepool instead");
	}

	/* Silently clip thread counts */
	if (ndownthreads < 1)
		ndownthreads = 1;
	if (nupthreads < 1)
		nupthreads = 1;

	/* The main thread is always available as a server thread */
	if (debugging) {
		printf("ndownthreads=%d, nupthreads=%d, nservers=%d\n",
			ndownthreads, nupthreads, nservers);
	}
	
	/* initialise object module */
	obj_init();

	/* Create the job queues */
	setupqueues();
	
	if ((err = getsuper(publish_dir, run_name, &supercap)) == STD_OK) {
	    /* supercap is already published, so there should also be an 
	     * adminstration file available.
	     */
	    if (!obj_read(admin_dir, run_name, &supercap)) {
		fatal("could not initialise from admin file");
	    }
	} else {
	    if (err == STD_NOTFOUND && autoinit) {
		publish_supercap(publish_dir, run_name);
		make_publish_pooldir();
		printf("published supercap in %s/%s\n", publish_dir, run_name);
	    } else {
		fatal("no -i flag given and cannot get supercap '%s/%s' (%s)",
		      publish_dir, run_name, err_why(err));
	    }
	}
	
	refresh_adminfile();
	
	startservers();
	
	/* Start orphan killing job if required */
	if (killorphans)
		procgcjob((char *)0);
	
	/* Start another server (we've no other use for this thread) */
	callserver();
	
	/*NOTREACHED*/
}

/* Initialize the job queues */

static void
setupqueues()
{
	upjobs = tj_create(JOBQUEUESTACK, nupthreads);
	if (upjobs == NULL)
		fatal("can't create upjobs");
	
	downjobs = tj_create(JOBQUEUESTACK, ndownthreads);
	if (downjobs == NULL)
		fatal("can't create downjobs");
}


/* Start server threads */

static void
startservers()
{
	int i;
	
	/* Start one less server than requested; the main thread becomes
	   the last server. */
	
	for (i = 1; i < nservers; ++i) {
		/* ml_runsvr needs a large stack... */
		if (!thread_newthread(callserver, SERVERSTACK, (char *)NULL,0))
			error("can't start server thread");
	}
}


/* Call the server main loop */

static void
callserver()
{
	extern port Priv_port; /* initialised by obj_init */
	errstat err;
	errstat ml_runsvr();
	
	(void) timeout(servertimeout);
	err = ml_runsvr(&Priv_port);
	fatal("ml_runsvr returned unexpectedly (%s)", err_why(err));
}



/* Dump status to buffer */

char *
status_header(buf, end)
char *buf;
char *end;
{
    char *p = buf;

    (void) strncpy(p, "HOST       STAT SINCE LREPL LPOLL   ",
		   (size_t)(end - p));
    p = strchr(p, '\0');
    (void) strncpy(p, "NPOLL  NBEST  NCONS    MIPS   NRUN   MBYTE\n",
		   (size_t)(end - p));
    p = strchr(p, '\0');
    return p;
}

char *
status_host(buf, end, p)
char *buf;
char *end;
struct hostinfo *p;
{
    if (buf != NULL) {
	if (end - buf > 100) {
	    /* 100 characters is enough to write info for one host */
	    sprintf(buf, "%-10s %-4s",
		    p->hi_name, p->hi_up ? "UP" : "DOWN");
	    buf = strchr(buf, '\0');
	    buf = prsince(buf, end, p->hi_lastflip);
	    buf = prsince(buf, end, p->hi_lastpoll);
	    buf = prsince(buf, end, p->hi_lastrepl);
	    sprintf(buf, " %7ld %6ld %6ld",
		    p->hi_npolls, p->hi_nbest, p->hi_nconsidered);
	    buf = strchr(buf, '\0');
	    if (p->hi_up) {
		long cpu = p->hi_cpuspeed;
		long run = p->hi_runnable;
		long mem = p->hi_freebytes;
		sprintf(buf, " %3ld.%03ld %2ld.%03ld %3ld.%03ld",
			cpu/(1024*1000), cpu/1024%1000,
			run/1000, run%1000,
			mem/(1024*1000), mem/1024%1000);
		buf = strchr(buf, '\0');
	    }
	    *buf++ = '\n';
	} else {
	    (void) strncpy(buf, "\n[truncated]\n", (size_t)(end - buf));
	    buf = NULL;
	}
    }
    return buf;
}

static char *
status_arch(buf, end, a)
char *buf;
char *end;
struct archdir *a;
{
    struct hostcap *hc;

    for (hc = a->ad_host; hc != NULL; hc = hc->hc_next) {
	buf = status_host(buf, end, hc->hc_info);
    }
    return buf;
}

char *
status_pool(buf, end, p)
char *buf;
char *end;
struct pooldir *p;
{
    struct archdir *a;

    for (a = p->pd_arch; a != NULL; a = a->ad_next) {
	buf = status_arch(buf, end, a);
    }
    if (p->pd_enabled == 0) {
	(void) sprintf(buf, "\n*** Pool disabled ***\n");
	buf = strchr(buf, '\0');
    }
    return buf;
}

char *
status_all(buf, end)
char *buf;
char *end;
{
    struct hostinfo *hi;

    for (hi = First_host; hi != NULL; hi = hi->hi_next) {
	buf = status_host(buf, end, hi);
    }
    return buf;
}

/* Helper for status_host(): nicely print the time elapsed since a certain
   time stamp.  Prints a space plus five characters */

static char *
prsince(buf, end, n)
	char *buf;
	char *end;
	unsigned long n; /*Time stamp, in milliseconds since our kernel is up*/
{
	if (buf == NULL) {
		return NULL;
	}
	if (n == 0) {
		(void) strncpy(buf, " -----", (size_t)(end - buf));
		return strchr(buf, '\0');
	}
	else {
		if (end - buf < 20) {
			return NULL;
		}

		n = (sys_milli() - n) / 1000;
		/* Now n is in seconds */
		if (n < 300)
			sprintf(buf, " %4lds", n);
		else if ((n /= 60) < 24*60) /* Now n is in minutes */
			sprintf(buf, " %2ld:%02ld", n/60, n%60);
		else if ((n /= 60) < 10*24) /* Now n is in hours */
			sprintf(buf, " %1ldd%02ldh", n/24, n%24);
		else /* Still in hours */
			sprintf(buf, " %4ldd", n/24);
		return strchr(buf, '\0');
	}
}


/* Reschedule job, after a server thread found it crashed */

updownflip(p)
	struct hostinfo *p;
{
	if (tj_cancel(upjobs, pollup, (char *)p) == STD_OK)
		adddownpoll(p, 0L);
}


/* Reschedule job, after a server thread started a new job */

upreschedule(p)
	struct hostinfo *p;
{
	errstat err, tj_timeleft();
	long left;

	err = tj_timeleft(upjobs, pollup, (char *)p, &left);
	if (err == STD_OK && left > pollstartdelay + 200L) {
		if (tj_cancel(upjobs, pollup, (char *)p) == STD_OK)
			adduppoll(p, pollstartdelay);
	}
}


/* Add a poll job to the down queue.  Prints nice message if failure */

void
adddownpoll(p, delay)
	struct hostinfo *p;
	long delay;
{
	addjob(downjobs, polldown, p, delay,
					"can't add job to down jobs queue");
}


/* Same for up queue */

static void
adduppoll(p, delay)
	struct hostinfo *p;
	long delay;
{
	addjob(upjobs, pollup, p, delay, "can't add job to up jobs queue");
}


/* Helper for above two: add a job to a queue.
   The message parameter is printed when an error occurs */

void
addjob(tj, func, p, delay, msg)
	struct tjobset *tj;
	void (*func)();
	struct hostinfo *p;
	long delay;
	char *msg;
{
	errstat err;
	
	if ((err = tj_addjob(tj, func, (char *)p, delay)) != STD_OK)
		error("%s (%s)", msg, err_why(err));
}


/* Poll a host that is assumed to be down.
   This is a job in the down job queue.
   If the host is still down, re-enter the same queue with a delay;
   otherwise, enter the up job queue */

#define INFO_LEN	16	/* enough, we only check the first char */

static void
polldown(arg)
	char *arg;
{
	char infobuf[INFO_LEN];
	int len;
	struct hostinfo *p = (struct hostinfo *)arg;
	errstat err;
	
	/*
	 * First try a std_info on the hostcap itself to see if the
	 * machine is possibly up.  If std_info returns STD_OK or STD_OVERFLOW,
	 * and the info string indicates a host directory (whose std_info
	 * string starts with a '%' character), we make sure it has a process
	 * server by looking up PROCESS_SVR_NAME.
	 *
	 * The reason for not doing the latter right away is that the Soap
	 * stubs may retry a couple of times, resulting in poor run server
	 * performance when several machines are down (especially during
	 * startup!).
	 */
	(void) timeout(polldowntimeout);
	p->hi_lastpoll = sys_milli();
	err = std_info(&p->hi_hostcap, infobuf, INFO_LEN, &len);
	if (err == STD_OVERFLOW) { /* doesn't matter */
		len = INFO_LEN;
		err = STD_OK;
	}

	if (err == STD_OK) {
		if (len > 1 && infobuf[0] == '%') {
			/* now see if it really has a process server */
			err = dir_lookup(&p->hi_hostcap, PROCESS_SVR_NAME,
					 &p->hi_proccap);
		} else {
			/* probably a bootstrap kernel (without proc server) */
			err = STD_COMBAD;
		}
	}

	if (err != STD_OK) {
		/* Still down */
		p->hi_npolls++;
		adddownpoll(p, polldowndelay);
		return;
	}

	p->hi_lastrepl = sys_milli();
	pollup((char *)p);
}

/* Poll a host that is assumed to be up.
   The argument must be in the up queue.
   If we suspect the host is no longer down, remove it from the up queue
   and insert it in the down queue */

static void
pollup(arg)
	char *arg;
{
	struct hostinfo *p = (struct hostinfo *)arg;
	errstat err;
	
	(void) timeout(polluptimeout);
	p->hi_lastpoll = sys_milli();
	err = pro_sgetload(&p->hi_proccap,
			   &p->hi_cpuspeed,
			   &p->hi_runnable,
			   &p->hi_freebytes,
			   &p->hi_execcap);
	if (err != STD_OK && debugging >= 2) {
		error("Host %s pro_getload error (%s)\n",
		      p->hi_name, err_why(err));
	}
	p->hi_lastrepl = sys_milli();
	if (err == STD_OK) {
		/* Nice.  It's up.  Check again in a while. */
		if (!p->hi_up) {
			/* Used to be down.  Restart statistics. */
			setupsegcache(p);
			p->hi_up = 1;
			p->hi_lastflip = sys_milli();
			p->hi_npolls = 0;
		}
		p->hi_npolls++;
		adduppoll(p, pollupdelay);
		return;
	}
	/* Ouch, it seems it has crashed.  Change queues. */
	if (p->hi_up) {
		/* Used to be up.  Restart statistics. */
		p->hi_up = 0;
		p->hi_lastflip = sys_milli();
		p->hi_npolls = 0;
	}
	p->hi_npolls++;
	adddownpoll(p, pollcrashdelay);
}

/*
 * Error reporting
 */

#ifdef __STDC__
#include <stdarg.h>
#define va_dostart(ap, format)	va_start(ap, format)
#else
#include <varargs.h>
#define va_dostart(ap, format)	va_start(ap)
#endif

#ifdef __STDC__
void error(char *format, ...)
#else
/*VARARGS1*/
void error(format, va_alist) char *format; va_dcl
#endif
{
    /* non fatal error */
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "%s: ", progname);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

#ifdef __STDC__
void fatal(char *format, ...)
#else
/*VARARGS1*/
void fatal(format, va_alist) char *format; va_dcl
#endif
{
    /* report fatal error and exit */
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "%s: fatal error: ", progname);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

