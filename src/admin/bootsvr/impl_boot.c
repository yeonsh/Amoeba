/*	@(#)impl_boot.c	1.6	94/04/06 11:39:00 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "semaphore.h"
#include "monitor.h"
#include "svr.h"

/*
 *	Implementations of boot-specific requests
 */

/*ARGSUSED*/
errstat
impl_boot_shutdown(_hdr, _obj)
    header *_hdr;
    obj_ptr _obj; /* act_f return value */
{
    int i, downs;
    static mutex mu;
    extern semaphore n_timers;

    if (_obj != NULL) return STD_DENIED;	/* require the supercap */
    mu_lock(&mu);
    if (closing) {
	mu_unlock(&mu);
	return STD_NOTNOW;			/* Fail: we cannot wait */
    }
    closing = 1;
    MU_CHECK(mu);
    mu_unlock(&mu);
    if (verbose || debugging) prf("%nshutdown\n");
    /* Do (local?) transactions with other threads to stop them */
    timeout((interval) 10000);
    for (i = N_SVR_THREADS; --i; ) {
	errstat err;
	err = boot_shutdown(&putcap);
	if (err != STD_OK && err != STD_NOTNOW)
	    prf("%nerror \"%s\" on closing\n", err_why(err));
	if (err == RPC_NOTFOUND) {
	    prf("%nOne server-thread not going away (!?)\n");
	    break;
	}
    }
    /* Wait for timerthreads */
    downs = N_TIMER_THREADS;
    while (downs > 0) downs -= sema_mdown(&n_timers, downs);
    return STD_OK;
} /* impl_boot_shutdown */

/*
 *	Re-read the configuration data, also called on
 *	initialisation (which is really starting from an
 *	empty configuration and reinitialising it)
 */
/*ARGSUSED*/
errstat
impl_boot_reinit(_hdr, _obj, last_line_read, success, n_conf)
    header *_hdr;
    obj_ptr _obj; /* act_f return value */
    int *last_line_read, *success, *n_conf;
{
    extern void stopscan();
    extern errstat NewConf();
    extern char parsstate[];
    extern int LineNo, Nconf;
    extern boot_data confs[];

    if (_obj != NULL) return STD_DENIED;	/* Require supercap */
    if (verbose || debugging) prf("%nReinit\n");
    if (!startscan()) return STD_NOTFOUND;
    *success = !yyparse();
    if (*success) (void) strcpy(parsstate, "configuration ok\n");
    *last_line_read = LineNo;
    *n_conf = Nconf;
    stopscan();
    return *success ? NewConf(confs, Nconf) : STD_SYSERR;
} /* impl_boot_reinit */

/*ARGSUSED*/
errstat
impl_boot_setio(_hdr, _obj, io)
    header *_hdr;
    obj_rep *_obj;
    capability *io;
{
    static capability io_cap;
    extern capability *outcap;
    if (_obj != NULL) return STD_DENIED;
    if (!isattycap(io)) return STD_ARGBAD;
    io_cap = *io;
    outcap = &io_cap;
    prf("%nSwitching to new I/O caps\n");
    return STD_OK;
} /* impl_boot_setio */

/*ARGSUSED*/
errstat
impl_boot_conf(_hdr, _obj, confname, max_confname, on)
    header *_hdr;
    obj_rep *_obj;
    char confname[];
    int max_confname;
    int on;
{
    int i;
    if (_obj != NULL) return STD_DENIED;
    if (on < 0 || on > 1) return STD_ARGBAD;
    if (confname[0] == '\0') return STD_NOTFOUND;
    for (i = 0; i < MAX_OBJ; ++i) {
	if (strncmp(confname, objs[i].or.or_data.name, max_confname) == 0) {
	    mu_lock(&objs[i].or_lock);
	    if (on) {
		/* poll and/or boot it soon, when needed! */
		objs[i].or.or_nextpoll = objs[i].or.or_nextboot = my_gettime();
		objs[i].or.or_data.flags &= ~BOOT_INACTIVE;
	    }
	    else objs[i].or.or_data.flags |= BOOT_INACTIVE;
	    MU_CHECK(objs[i].or_lock);
	    mu_unlock(&objs[i].or_lock);
	    return STD_OK;
	}
    }
    return STD_NOTFOUND;
} /* impl_boot_conf */

void
pr_env()
{
    char **p;
    int n = 0;
    extern char **environ;
    prf("%nenv dump\n");
    for (p = environ; p != NULL && *p != NULL; ++p) {
	prf("%nenv: %s\n", *p);
	++n;
    }
    prf("%nenv: %d strings\n", n);
} /* pr_env */

/*ARGSUSED*/
errstat
impl_boot_ctl(_hdr, _obj, option, msg, max_msg, len)
    header *_hdr;
    obj_ptr _obj; /* boot_act return value */
    char option; /* in */
    char *msg; /* out */
    int   max_msg;
    int  *len; /* out (h_extra) */
{
    static char xplain[] = "\
?: boot_ctl usage\n\
d: debug messages off\n\
D: show debugging messages\n\
e: print environ on svr's stdout\n\
f: list flags-values\n\
s: save state\n\
t: don't print time\n\
T: do print time\n\
v: verbose off\n\
V: verbose on\n";

    if (_obj != NULL) return STD_DENIED;
    switch (option) {
    case '?':
	(void) strncpy(msg, xplain, (size_t) max_msg);
	break;
    case 'd':	/* Debug off */
	debugging = 0;
	(void) strncpy(msg, "Debugging off\n", (size_t) max_msg);
	break;
    case 'D':	/* Debug on */
	debugging = 1;
	(void) strncpy(msg, "Debugging on\n", (size_t) max_msg);
	break;
    case 'e':	/* Dump environ */
	pr_env();
	(void) strncpy(msg, "the environment is dumped on console\n",
		       (size_t) max_msg);
	break;
    case 'f':
    	{
	    char msg_buf[80];

	    sprintf(msg_buf, "verbose=%d, debugging=%d, timeprint=%d\n",
		    verbose, debugging, TimePrint);
	    (void) strncpy(msg, msg_buf, (size_t) max_msg);
	}
	break;
    case 's':	/* Save */
	(void) strncpy(msg, "Gonna save state\n", (size_t) max_msg);
	SaveState = 1;
	break;
    case 't':
	(void) strncpy(msg, "time-print off\n", (size_t) max_msg);
	TimePrint = 0;
	break;
    case 'T':
	(void) strncpy(msg, "time-print on\n", (size_t) max_msg);
	TimePrint = 1;
	break;
    case 'v':
	verbose = 0;
	(void) strncpy(msg, "Verbose off\n", (size_t) max_msg);
	break;
    case 'V':
	verbose = 1;
	(void) strncpy(msg, "Verbose on\n", (size_t) max_msg);
	break;
    default:
	return STD_ARGBAD;
    }
    msg[max_msg - 1] = '\0';
    *len = strlen(msg);
    if (verbose || debugging) prf("%n(boot_ctl) %s", msg);
    return STD_OK;
} /* impl_boot_ctl */
