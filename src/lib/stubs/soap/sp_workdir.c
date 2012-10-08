/*	@(#)sp_workdir.c	1.3	96/02/27 11:18:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

extern capset _sp_rootdir;
#ifndef OWNDIR
extern capset _sp_workdir;
#endif

errstat
sp_get_workdir(work)
capset *work;
{
    errstat err;

    if ((err = sp_init()) != STD_OK) {
	return err;
    }

#ifdef OWNDIR
    err = sp_lookup(&_sp_rootdir, "work", work);
#else
    err = cs_copy(work, &_sp_workdir) ? STD_OK : STD_NOSPACE;
#endif

    return err;
}

errstat
sp_get_rootdir(root)
capset *root;
{
    errstat err;

    if ((err = sp_init()) != STD_OK) {
	return err;
    }

    return cs_copy(root, &_sp_rootdir) ? STD_OK : STD_NOSPACE;
}

