/*	@(#)embalm.c	1.4	96/02/27 13:13:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Code to preserve processes that died of an exception.
   If there is a directory /dev/dead, a copy of the dead process is
   stored there.  It may be possible to use a debugger to inspect it.
   (Resurrection would be much harder -- Amoeba doesn't support cryonics.) */

/* Amoeba .h files */
#include <amtools.h>
#include <module/proc.h>
#include <ajax/mymonitor.h>
#include <ampolicy.h>
#include "session.h"

errstat
embalm(pid, pd, pdlen)
	int pid;
	process_d *pd;
	int pdlen;
{
	errstat err;
	capability devdead;
	capability bullet;
	capability corpse;
	char namebuf[20];
	
	if ((err = name_lookup(DEF_CORE_DIR, &devdead)) != STD_OK) {
		warning("dumpcore: cannot lookup %s (%s)",
			DEF_CORE_DIR, err_why(err));
		return STD_OK;
	}
	if ((err = name_lookup(DEF_BULLETSVR, &bullet)) != STD_OK) {
		warning("dumpcore: cannot lookup %s (%s)",
			DEF_BULLETSVR, err_why(err));
		return err;
	}
	if ((err = pd_preserve(&bullet, pd, pdlen, &corpse)) < 0) {
		warning("dumpcore: pd_preserve failed (%s)", err_why(err));
		return err;
	}
	sprintf(namebuf, "%d", pid);
	if ((err = dir_append(&devdead, namebuf, &corpse)) != STD_OK) {
		(void) dir_delete(&devdead, namebuf);
		err = dir_append(&devdead, namebuf, &corpse);
		if (err != STD_OK)
		    warning("dumpcore: cannot append to %s (%s)",
			    DEF_CORE_DIR, err_why(err));
		
	}
	if (err == STD_OK && debugging)
		fprintf(stderr, "dumpcore: dumped core to %s/%s\n",
			DEF_CORE_DIR, namebuf);
	return err;
}
