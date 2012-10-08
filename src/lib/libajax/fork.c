/*	@(#)fork.c	1.7	96/02/27 10:59:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* fork() POSIX 3.1.1
 * modified apr 22 93 Ron Visser
 * TO DO: set signal server cap? (now relies on sigsvr waking up) 
 * TO DO: don't hold a mutex while forking (it's a miracle this works...) 
 */

#include "ajax.h"
#include "sesstuff.h"
#include "sigstuff.h"
#include "fdstuff.h"
#include "module/proc.h"
#include "module/rnd.h"

static capability nullcap; /* Constant */

extern void uniqport_reinit();
extern errstat ses_runsvr();

static void
specify_runsvr(session)
capability *session;
/*
 * tell session svr which runsvr/pooldir to use when starting the forked child
 */
{
	static capability current_runsvr, current_pooldir;
	capability runsvrcap, *runsvr;
	capability pooldircap, *pooldir;

	if (exec_get_runsvr(&runsvrcap) == STD_OK) {
		runsvr = &runsvrcap;
	} else {
		runsvr = &nullcap;	/* lets session svr take default */
	}
	if (exec_get_pooldir(&pooldircap) == STD_OK) {
		pooldir = &pooldircap;
	} else {
		pooldir = &nullcap;	/* lets session svr take default */
	}

	/* only tell session server if something has changed */
	if (memcmp(pooldir, &current_pooldir, sizeof(capability)) != 0 ||
	    memcmp(runsvr,  &current_runsvr,  sizeof(capability)) != 0)
	{
		if (ses_runsvr(session, runsvr, pooldir) == STD_OK) {
			current_runsvr = *runsvr;
			current_pooldir = *pooldir;
		}
	}
}

static int
fork_error(err)
int err;
{
  if (err == STD_NOMEM) 
	return ENOMEM;
  else 
	return EAGAIN;
}

pid_t
fork()
{
	capability *session;
	int pid;
	int err;
	static int reinit_rndsvr = 0;
	
	SESINIT(session);
	pid = prv_number(&session->cap_priv);
	MU_LOCK(&_ajax_forkmu); /* Must be unlocked in parent and child! */
	_ajax_shareall(); /* Move all bullet files into session server */

	specify_runsvr(session); /* specify the runsvr/pooldir to use */
	
	if (++reinit_rndsvr >= 5) {
		/* Every once in a while, we refresh the random number
		 * server cap.  If it has been replaced (e.g., because
		 * the file server was rebooted) we don't want our child
		 * to time out on it, when doing its first uniqport().
		 * That would be especially annoying in shells, since
		 * the timeout would be experienced by every new child.
	 	 */
		reinit_rndsvr = 0;
		rnd_defcap();
	}

	/* Signal ourselves, so the session server can fork a child */
	(void) _ajax_sigself(R_FORK);
	
	/* When we get here, we are either parent or child;
	   find out which by checking our owner capability */
	if ((err = _ajax_getowner(session)) != STD_OK)
		ERR(EAGAIN, "fork: ajax_getowner failed");
	if (prv_number(&session->cap_priv) != pid) {
		/* Child */
		_ajax_forklevel++;
		/* We want new random ports, independent of the parent! */
		uniqport_reinit();
		(void) ses_child(session, /*dummy*/ &nullcap);
		MU_UNLOCK(&_ajax_forkmu);
		sigemptyset(&_ajax_sigpending);
		return 0;
	}
	else {
		/* Parent */
		err= ses_parent(session, &pid);
		MU_UNLOCK(&_ajax_forkmu);
		if (err != 0)
			ERR(fork_error(err), "fork: not forked");
		return pid;
	}
}
