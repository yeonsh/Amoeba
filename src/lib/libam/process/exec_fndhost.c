/*	@(#)exec_fndhost.c	1.5	96/02/27 11:03:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** exec_findhost()/exec_multi_findhost()
**
**	Find a pool processor suitable to execute a (one of a set of)
**	process descripor(s).
**	If the pooldir server is up, the fastest way is to ask it for a
**	suitable processor, using pool_multi_findhost().
**
**	Failing that, we scan the directory <POOL_DIR>/<arch>
**	until we find a (quickly) responding processor.
**	The results of this scan are cached for future use and the
**	responding processors are used on a round-robin basis.
**      XXX Most of this can be taken out once we can trust the run
**      server to be up at all times.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "exec_fndhost.h"
#include "module/name.h"
#include "direct/direct.h"
#include "module/direct.h"
#include "module/stdcmd.h"
#include "module/rnd.h"
#include "module/mutex.h"
#include "module/ar.h"
#include "module/buffers.h"
#include "module/proc.h"
#include "ampolicy.h"
#include "run.h"

/* Debugging macro */
#ifdef DEBUG
#define dbprintf(list)	fprintf list
#else
#define dbprintf(list)	{}
#endif

/* External and forward declarations */

static errstat pick_host();


/*
 * do_findhost() marhalls a process descriptor list and calls the runsvr
 * using run_multi_findhost().
 */
static errstat
do_findhost(runsvr, pd_list, npd, pd_which, hostcap, hostname)
capability *runsvr;		   /* processing the request */
process_d  *pd_list[];		   /* array of pd ptrs */
int         npd;		   /* length of the pd_list */
int 	   *pd_which;		   /* index of pd chosen */
capability *hostcap;		   /* process server of host chosen */
char 	    hostname[PD_HOSTSIZE]; /* name of host chosen */
{
    char    *pdbuf, *bufp;
    long     tot_pd_size;
    int	     pd_index;
    interval save_timeout;
    errstat  err;

    /* determine buffer size enough to hold all pd's */
    tot_pd_size = 0;
    for (pd_index = 0; pd_index < npd; pd_index++) {
	tot_pd_size += pd_size(pd_list[pd_index]);
    }

    pdbuf = (char *) malloc((size_t) tot_pd_size);
    if (pdbuf == NULL) {
	return STD_NOSPACE;
    }

    /* put them all in the buffer */
    bufp = pdbuf;
    for (pd_index = 0; pd_index < npd; pd_index++) {
	bufp = buf_put_pd(bufp, pdbuf+tot_pd_size, pd_list[pd_index]);
    }
    if (bufp == NULL) {
	/* should not happen, as long as pd_size() tells the truth */
	free(pdbuf);
	return STD_SYSERR;
    }

    save_timeout = timeout((interval) 2000);
    err = run_multi_findhost(runsvr, npd, pdbuf, (int)(bufp - pdbuf),
			     pd_which, hostname, hostcap);
    (void) timeout(save_timeout);

    if (err == STD_OK) {
	if (*pd_which < 0 || *pd_which >= npd) { /* sanity check */
	    err = STD_SYSERR;
	}
    }

    free(pdbuf);
    return err;
}

/*
 * Functions giving control over the default pool directory/run server
 * to be used.
 *
 * void exec_set_runsvr(capability *runcap)
 * void exec_set_pooldir(capability *pooldir)
 *
 * errstat exec_get_runsvr(capability *runcap)
 * errstat exec_get_pooldir(capability *pooldir)
 */

static capability Runcap;
static capability Poolcap;
static int Runcap_valid = 0;	/* Runcap not yet initialised */
static int Poolcap_valid = 0;	/* Poolcap not yet initialised */

void
exec_set_runsvr(runcap)
capability *runcap;
{
    if (runcap == NULL) {
	Runcap_valid = 0;
    } else {
	Runcap = *runcap;
	Runcap_valid = 1;
    }
}

void
exec_set_pooldir(poolcap)
capability *poolcap;
{
    if (poolcap == NULL) {
	Poolcap_valid = 0;
    } else {
	Poolcap = *poolcap;
	Poolcap_valid = 1;
    }
}

errstat
exec_get_runsvr(runsvr)
capability *runsvr;
{
    errstat retval;

    if (!Runcap_valid) {
	char      *run_name;
	capability runcap;

	/* environment variable RUN_SERVER overrides default runserver */
	if ((run_name = getenv("RUN_SERVER")) == NULL) {
	    run_name = DEF_RUNSVR_POOL;
	}

	if ((retval = name_lookup(run_name, &runcap)) == STD_OK) {
	    exec_set_runsvr(&runcap);
	}
    } else {
	retval = STD_OK;
    }

    if (retval == STD_OK) {
	*runsvr = Runcap;
    }
    return retval;
}

errstat
exec_get_pooldir(pooldir)
capability *pooldir;
{
    errstat retval;

    if (!Poolcap_valid) {
	capability poolcap;

	if ((retval = name_lookup(POOL_DIR, &poolcap)) == STD_OK) {
	    exec_set_pooldir(&poolcap);
	}
    } else {
	retval = STD_OK;
    }

    if (retval == STD_OK) {
	*pooldir = Poolcap;
    }
    return retval;
}

errstat
exec_multi_findhost(runsvr, pooldir, pd_list, npd, pd_chosen, hostcap,hostname)
capability *runsvr;	/* run server capability or NULL */
capability *pooldir;	/* pool directory to fall back on or NULL */
process_d  *pd_list[];	/* array of process discriptor pointers */
int         npd;	/* its length */
int 	   *pd_chosen;	/* OUT: index process desciptor chosen */
capability *hostcap;	/* OUT: process server chosen */
char 	    hostname[PD_HOSTSIZE];
			/* OUT: name of host chosen */
{
    capability runsvrcap;
    errstat err;

    if (npd < 1) {		/* silly */
	return STD_ARGBAD;
    }

    if (runsvr == NULL) {	/* use default, if available */
	if (exec_get_runsvr(&runsvrcap) == STD_OK) {
	    runsvr = &runsvrcap;
	}
    }

    if (runsvr != NULL && !NULLPORT(&runsvr->cap_port)) {
	err = do_findhost(runsvr, pd_list, npd, pd_chosen, hostcap, hostname);
    } else {
	err = STD_NOTFOUND;
    }

    if (err != STD_OK) {
	int pd_index;

	dbprintf((stderr, "got no host from run server (%s)\n", err_why(err)));

	/* We have to pick a host ourselves.  It should be suitable for one
	 * of the pd's provided.  Just try them in the order provided.
	 */
	for (pd_index = 0; pd_index < npd; pd_index++) {
	    char *arch = pd_arch(pd_list[pd_index]);

	    if ((err = pick_host(pooldir, arch, hostname, hostcap)) == STD_OK){
		*pd_chosen = pd_index;
		break;
	    }
	}
    }

    return err;
}

/*
 * Old interface: the exec_findhost() is just the npd = 1 case
 * of exec_multi_findhost() with default runsvr/pool values.
 */
errstat
exec_findhost(pd, hostcap_ret)
process_d  *pd;
capability *hostcap_ret;
{
    char    hostbuf[PD_HOSTSIZE];
    int	    pd_index;
    errstat err;

    err = exec_multi_findhost((capability *)NULL, (capability *)NULL,
			      &pd, 1, &pd_index, hostcap_ret, hostbuf);
    if (err != STD_OK) {
	dbprintf((stderr, "exec_findhost failure (%s)", err_why(err)));
    }

    return err;
}

/*
 * Routines to fall back on in case we cannot find a run server.
 */


/* Name and process server capability for all machines in arch dir: */
static struct proc_data {
    capability server;
    char       name[PD_HOSTSIZE];
} *Proc_list = NULL;

static int Num_procs;		/* Entries in use in Proc_list array */
static mutex Proc_data_mutex;	/* disallow concurrent access to Proc_list */



/* Read the pool directory and initialize the list of processor names and
 * process server caps.  Called by exec_findhost, whenever the run server is
 * not being used, and the architecture or pooldir changes:
 */
static errstat
readpool(arch, pooldir)
char	   *arch;
capability *pooldir;
{
    errstat	      err;
    char	      namebuf[256];
    char	     *proc;
    capability	      archdir;
    struct dir_open  *dp;
    static int	      num_procs_allocd;   /* Num entries actually malloc'd */

    dbprintf((stderr, "readpool: scanning pool dir for arch %s\n", arch));

    /* Open <pooldir>/<arch> */
    if ((err = dir_lookup(pooldir, arch, &archdir)) != STD_OK) {
	dbprintf((stderr, "could not lookup arch dir %s/%s (%s)\n",
		  POOL_DIR, arch, err_why(err)));
	return FPE_NOPOOLDIR;
    }
    if ((dp = dir_open(&archdir)) == 0) {
	dbprintf((stderr, "could not open arch dir %s/%s (%s)\n",
		  POOL_DIR, arch, err_why(err)));
	return FPE_BADPOOLDIR;
    }

    if (Proc_list == NULL || num_procs_allocd <= 0) {
	num_procs_allocd = 10;
	if ((Proc_list = (struct proc_data *)malloc((size_t)num_procs_allocd *
				    sizeof(struct proc_data))) == NULL)
	    return STD_NOSPACE;
    }

    Num_procs = 0;
    while ((proc = dir_next(dp)) != NULL) {
	interval tout;
	
	/* skip run server entries, having a '.' in their name */
	if (strchr(proc, '.') != NULL) {
	    continue;
	}

	/* allocate more room if needed */
	if (Num_procs >= num_procs_allocd) {
	    struct proc_data *newlist;
	    int newsize = num_procs_allocd * 2;

	    newlist = (struct proc_data *)realloc(Proc_list,
				 (size_t)(newsize * sizeof(struct proc_data)));
	    if (newlist == NULL) {
		(void) dir_close(dp);
		return STD_NOSPACE;
	    } else {
		Proc_list = newlist;
		num_procs_allocd = newsize;
	    }
	}

	/* only add it if it has a process server entry */
	(void)sprintf(namebuf, "%s/%s", proc, PROCESS_SVR_NAME);
	tout = timeout((interval) 1000);
	err = dir_lookup(&archdir, namebuf, &Proc_list[Num_procs].server);
	(void) timeout(tout);

	if (err != STD_OK) {
	    dbprintf((stderr, "%s not added to host list\n", proc));
	} else {
	    dbprintf((stderr, "adding %s to process svr list\n", proc));
	    strncpy(Proc_list[Num_procs].name, proc, PD_HOSTSIZE);
	    Proc_list[Num_procs].name[PD_HOSTSIZE-1] = '\0'; /* nul-terminate*/
	    Num_procs++;
	}
    }
    (void) dir_close(dp);

    if (Num_procs == 0) {	/* no hosts of this arch available */
	return FPE_NONE;
    }

    return STD_OK;
}


static errstat
pick_host(pooldir, arch, hostname, procsvr)
capability *pooldir;
char	   *arch;
char 	    hostname[PD_HOSTSIZE];
capability *procsvr;
/* This function is called when we cannot find the run server;
 * we scan the directory <pooldir>/<arch> ourselves, and do a 
 * simple round-robin host selection.
 */
{
    static char         cached_arch[ARCHSIZE] = ""; /* arch of Proc_list    */
    static capability   cached_pooldir;		    /* pooldir of Proc_list */
    static unsigned int proc_counter = 0;	    /* for host selection   */

    struct proc_data   *goodproc;		    /* good host found	    */
    int			available;
    capability		pooldircap;
    errstat		retval;
#   define Failure(err)	{ retval = err; goto Exit; /* unlocks mutex */ }

    mu_lock(&Proc_data_mutex);	/* disallow concurrent access to Proc_list */

    if (pooldir == NULL) {
	/* Get default.  It should always be available to fall back on. */
	if (exec_get_pooldir(&pooldircap) != STD_OK) {
	    Failure(FPE_NOPOOLDIR);
	}
	pooldir = &pooldircap;
    }

    /* (Re-)initialize list of process-server capabilities if caller gives
     * us a new pooldir, or if caller asks a host for a different architecture
     * than we've most recently read.
     */
    if ((memcmp(pooldir, &cached_pooldir, sizeof(capability)) != 0) ||
	(strcmp(arch, cached_arch) != 0)) {
	errstat err;

	dbprintf((stdout, "pick_host: scan arch %s of pooldir %s\n",
		  arch, ar_cap(pooldir)));
    
	if ((err = readpool(arch, pooldir)) != STD_OK) {
	    Failure(err);
	}

	/* Record pooldir and architecture associated with Proc_list */
	cached_pooldir = *pooldir;
	(void) strcpy(cached_arch, arch);
    }

    if (proc_counter == 0) {
	/* Randomise to avoid the syndrom that every newly created process
	 * starts executing commands on exactly the same machine.
	 */
	(void) rnd_getrandom((char *) &proc_counter, sizeof(proc_counter));
    }

    /* Find next responding host in Proc_list */
    goodproc = NULL;
    for (available = Num_procs; available > 0; available--) {
	struct proc_data *proc = &Proc_list[proc_counter++ % Num_procs];
	int     len;
	errstat err;

	err = std_info(&proc->server, (char *) NULL, 0, &len);
	if (err == STD_OK || err == STD_OVERFLOW) {
	    goodproc = proc;	/* found one that responded */
	    break;
	} else {
	    dbprintf((stderr, "pick_host: `%s' not responding\n", proc->name));
	}
    }

    if (goodproc == NULL) { /* didn't find one */
	Failure(FPE_NONE);
    }

    (void) strcpy(hostname, goodproc->name);
    *procsvr = goodproc->server;
    retval = STD_OK;

Exit:
    mu_unlock(&Proc_data_mutex);
    return retval;
}

