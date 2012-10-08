/*	@(#)ajappend.c	1.3	94/04/07 10:22:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Append a name to a directory */
/* XXX umask? */

#include "ajax.h"

errstat
_ajax_append(dir, path, pcap)
	capability *dir;
	const char *path;
	capability *pcap;
{
	long cols[SP_MAXCOLUMNS];
	int ncols;
	capset csdir, csnew;
	errstat err;
	
	if ((err = _ajax_csorigin(dir, path, &csdir)) != STD_OK)
		return err;
	if (!cs_singleton(&csnew, pcap)) {
		cs_free(&csdir);
		return STD_NOSPACE;
	}
	ncols = sp_mask(SP_MAXCOLUMNS, cols); /* size of cols array */
	err = sp_append(&csdir, path, &csnew, ncols, cols);
	cs_free(&csnew);
	cs_free(&csdir);
	return err;
}
