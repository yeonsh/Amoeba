/*	@(#)sp_mkdir.c	1.4	94/04/07 10:19:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** Function to make a directory.
** Masks are set by sp_mask() from environment SPMASK, default all 0xFF.
** Column names are set from third parameter; defaults owner,group,other.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"

static char *def_colnames[] = { "owner", "group", "other", 0 };

#ifndef __STDC__
#define const /**/
#endif

errstat
sp_mkdir(start, path, colnames)
	capset *start;
	char *path;
	char **colnames;
{
	long cols[SP_MAXCOLUMNS];
	int ncols;
	char *name;
	capset csparent, csnew;
	errstat err;
	
	name = path;
	err = sp_traverse(start, (const char **) &name, &csparent);
	if (err != STD_OK) {
		return err;
	}
	if (colnames == 0)
		colnames = def_colnames; /* XXX or from environment? */
	if ((err = sp_create(&csparent, colnames, &csnew)) != STD_OK) {
		cs_free(&csparent);
		return err;
	}
	ncols = sp_mask(SP_MAXCOLUMNS, cols); /* size of cols array */
	if ((err = sp_append(&csparent, name, &csnew, ncols, cols)) != STD_OK) {
		sp_discard(&csnew);
	}
	cs_free(&csparent);
	cs_free(&csnew);
	return err;
}
