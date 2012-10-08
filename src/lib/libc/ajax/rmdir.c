/*	@(#)rmdir.c	1.4	94/04/07 10:31:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix rmdir() function */

#include "ajax.h"


static int
sp_rmdir(start, path)
	capset *start;
	char *path;
{
	char *name;
	capset csparent, csdir;
	errstat err;
	int i;
	
	name = path;
	err = sp_traverse(start, (const char **) &name, &csparent);
	if (err != STD_OK) {
		TRACENUM("sp_rmdir: sp_traverse err", err);
		goto cleanup0;
	}
	if ((err = sp_lookup(&csparent, name, &csdir)) != STD_OK) {
		TRACENUM("sp_rmdir: sp_lookup err", err);
		goto cleanup1;
	}
	for (i = 0; i < csdir.cs_final; ++i) {
		errstat err1;
		if (!csdir.cs_suite[i].s_current)
			continue;
		err1 = std_destroy(&csdir.cs_suite[i].s_object);
		if (err1 != STD_OK) {
			TRACENUM("sp_rmdir: std_destroy err", err1);
			err = err1;
		}
	}
	if (err == STD_OK)
		err = sp_delete(&csparent, name);
	cs_free(&csdir);
cleanup1:
	cs_free(&csparent);
cleanup0:
	return err;
}


int
rmdir(path)
	const char *path;
{
	errstat err;
	if ((err = sp_rmdir(SP_DEFAULT, path)) != STD_OK)
		ERR(_ajax_error(err), "rmdir: sp_rmdir failed");
	return 0;
}
