/*	@(#)devproc.c	1.3	96/02/27 13:13:54 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Server for process directory (/dev/proc) */
#include <amtools.h>
#include <ampolicy.h>
#include <file.h>
#include <stdlib.h>
#include <stdcom.h>
#include <stderr.h>
#include <ajax/mymonitor.h>
#include <module/rnd.h>
#include <capset.h>
#include <soap/soap.h>
#include <module/proc.h>
#include <thread.h>
#include "devproc.h"
#include "session.h"

#define NTHREAD		2	      		/* Number of server threads */
#define INFO		"process directory"	/* std_info return string */
#define DEVPROC		"/dev/proc"		/* published name */


extern port _sp_getport;
extern capset _sp_rootdir;

static capset sp_session_rootdir;
static capset proc_dir;

/*ARGSUSED*/
static void
devprocsvr(param, size)
char *param;
int   size;
{
    header h;
    bufsize n, repsize;
    errstat err;
    bufptr reqbuf, repbuf;

    reqbuf = (bufptr) malloc((size_t) SP_BUFSIZE);
    repbuf = (bufptr) malloc((size_t) SP_BUFSIZE);
    if (reqbuf == NULL || repbuf == NULL) {
	warning("cannot allocate directory transaction buffer");
	if (reqbuf != NULL) free((_VOIDSTAR) reqbuf);
	if (repbuf != NULL) free((_VOIDSTAR) repbuf);
	return;
    }

    for (;;) {
	h.h_port = _sp_getport;
	n = getreq(&h, reqbuf, SP_BUFSIZE);
	if (ERR_STATUS(n)) {
	    err = ERR_CONVERT(n);
	    if (err == RPC_ABORTED) {
		MON_EVENT("directory thread: getreq aborted");
	    } else {
		warning("directory thread: getreq failed (%s)", err_why(err));
	    }
	    continue;
	}

	/* Just pass the command to the sp_impl.c code */
	repsize = sp_trans(&h, reqbuf, (int) n, repbuf, SP_BUFSIZE);
	putrep(&h, repbuf, repsize);
    }
}

/* No reason to give "group" members special privilages (at least I don't
 * trust them a bit ..), so for the moment just support two columns in the
 * session server.  Further, the owner must not delete entries since that
 * doesn't result in the death of the process, merely that the owner can
 * no longer reach it or find out its status.
 */
#define PROC_NCOLS	2

static rights_bits procdir_cols[] = { 0x0F, 0x02, 0x02 };
static rights_bits process_cols[] = { PSR_ALL, PSR_READ, 0x00 };

/*ARGSUSED*/
static void
init_devprocsvr()
{
    static int inited;

    if (!inited) {
	/* Create an empty in-core "proc" directory.
	 * That is the directory we will publish on Soap as /dev/proc.
	 * Here we only publish it in the in-core root.
	 */
	static char *colnames[] = { "owner", "other", 0 };
	errstat err;

	err = sp_create(&sp_session_rootdir, colnames, &proc_dir);
	if (err != STD_OK) {
	    fatal("cannot create internal process directory", err_why(err));
	}
	err = sp_append(&sp_session_rootdir, "proc", &proc_dir,
			PROC_NCOLS, procdir_cols);
	if (err != STD_OK) {
	    fatal("cannot install internal process directory (%s)",
		  err_why(err));
	}

	inited = 1;
    }
}

errstat
start_devprocsvr()
{
    int i;

    init_devprocsvr();
    /* sp_init(); */
    for (i = 1; i <= NTHREAD; ++i) {
	if (!thread_newthread(devprocsvr, 16000, NILBUF, 0)) {
	    warning("cannot fork directory server thread");
	    if (i == 0)
		return STD_NOSPACE;
	}
    }
    return STD_OK;
}

errstat
publish_devproccap()
{
    errstat err;

    (void) name_delete(DEF_SESSION_PROC_DIR);
    err = sp_append(&_sp_rootdir, DEF_SESSION_PROC_DIR, &proc_dir,
		    PROC_NCOLS, procdir_cols);

    if (err != STD_OK) {
	warning("cannot publish directory cap %s (%s)",
		DEF_SESSION_PROC_DIR, err_why(err));
    }
    return err;
}

errstat
unpublish_devproccap()
{
    errstat err;

    if ((err = name_delete(DEF_SESSION_PROC_DIR)) != STD_OK) {
	warning("cannot delete directory cap %s (%s)",
		DEF_SESSION_PROC_DIR, err_why(err));
    }
    return err;
}

void
init_dirsvr()
{
    errstat err;
    capability *rootcap;

    /* initialize the in-core directory server */
    if ((err = sp_init()) != STD_OK) {
	fatal("cannot initialize internal directory server (%s)",
	      err_why(err));
    }

    /* Reset _sp_rootdir to let soap requests by the session server
     * itself go to the original directory server (Soap).  Another
     * possibility would be to initialize the our own directory root
     * with the capabilities in our root on Soap, but the current
     * approach is simpler and safer.
     */
    sp_session_rootdir = _sp_rootdir;

    if ((rootcap = dir_origin("/")) == NULL) {
	fatal("no ROOT in capability environment");
    }
    if (!cs_singleton(&_sp_rootdir, rootcap)) {
	fatal("cannot allocate capset for root");
    }
}

errstat
proc_publish(pid, replace, proccap)
int         pid;
int         replace;
capability *proccap;
{
    char proc_name[20];
    errstat err;
    capset proc_cs;

    if (!cs_singleton(&proc_cs, proccap)) {
	return STD_NOMEM;
    }

    sprintf(proc_name, "%d", pid);
    
    if (replace) {
	err = sp_replace(&proc_dir, proc_name, &proc_cs);
    } else {
	err = sp_append(&proc_dir, proc_name, &proc_cs,
			PROC_NCOLS, process_cols);
    }

    if ((replace && err == STD_NOTFOUND) ||
	(!replace && err == STD_EXISTS))
    {
	if (err == STD_EXISTS) {
	    (void) sp_delete(&proc_dir, proc_name);
	}
	err = sp_append(&proc_dir, proc_name, &proc_cs,
			PROC_NCOLS, process_cols);
    }

    cs_free(&proc_cs);
    return err;
}

errstat 
proc_unpublish(pid)
int pid;
{
    char proc_name[20];

    sprintf(proc_name, "%d", pid);
    return sp_delete(&proc_dir, proc_name);
}
