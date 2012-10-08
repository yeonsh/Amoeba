/*	@(#)dumpcore.c	1.4	96/02/27 10:10:31 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "svr.h"
#include "ampolicy.h"
#include "module/proc.h"
#include "monitor.h"
#include "module/direct.h"

static boot_ref core_dir;

SetCoreDir(dir)
    boot_ref dir;
{
    core_dir = dir;
} /* SetCoreDir */

void
dumpcore(obj, pd, pdlen)
    obj_ptr obj;
    process_d *pd;
    int pdlen;
{
    capability dir, core;
    errstat err;
    char *bulletname;

    /* I wish I could test on bad ports here; but the capability is hidden */

    err = ref_lookup(&core_dir, &dir, (obj_rep *) NULL);
    if (err != STD_OK) {
	MON_EVENT("coredump: cannot lookup the dump directory");
	if (debugging || verbose)
	    prf("%ncannot lookup dir %s (%s)\n",
		BOOT_CORE_DUMP_DIR, err_why(err));
	return;
    }

    /*
     *	Find the bulletserver like exec_file does it
     */
    if ((bulletname = getenv("BULLET")) == NULL)
	bulletname = DEF_BULLETSVR;
    err = name_lookup(bulletname, &core);
    if (err == STD_NOTFOUND) {
	bulletname = "/bullet";
	err = name_lookup(bulletname, &core);
    }
    if (err != STD_OK) {
	MON_EVENT("coredump: no fileserver found");
	if (debugging || verbose)
	    prf("%ncannot lookup filesvr %s (%s)\n",
			DEF_BULLETSVR, err_why(err));
	return;
    } else if (debugging)
	prf("%ncoredump: using %s as fileserver\n", bulletname);

    if (debugging) prf("%ncore-len of %s: %d\n", obj->or.or_data.name, pdlen);

    err = pd_preserve(&core, pd, pdlen, &core);
    if (err != STD_OK) {
	MON_EVENT("coredump: cannot preserve");
	if (debugging || verbose)
	    prf("%ncannot preserve core (%s)\n", err_why(err));
	return;
    }
    err = badport(&dir) ?
	RPC_NOTFOUND : capsaid(&dir, dir_append(&dir, obj->or.or_data.name, &core));
    if (err != STD_OK) {	/*HIRO: it always return STD_ARGBAD*/
	if (debugging || verbose)
	    prf("%ncannot append coredump of %s (%s)\n",
			obj->or.or_data.name, err_why(err));
	MON_EVENT("coredump: cannot append capability");
	(void) std_destroy(&core);
    } else MON_EVENT("coredump: dumped");
} /* dumpcore */
