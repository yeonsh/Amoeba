/*	@(#)dir_spcompat.c	1.4	96/02/27 11:14:48 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "capset.h"
#include "soap/soap.h"

#define NCOLUMNS	3

char *_ds_columns[NCOLUMNS+1] = {
	"owner",
	"group",
	"other",
	0
};

int _ds_ncolumns;
long _ds_colmasks[NCOLUMNS];

void
_dir_soapcompat()
{
	static int inited;

	if (inited)
		return;
	inited++;
	_ds_ncolumns = sp_mask(NCOLUMNS, _ds_colmasks);
}

int
dir_set_colmasks(columns, ncols)
long *columns;
int ncols;
/* This function sets the default column masks used when appending names
 * with the dir/name interface.  It should be called when the masks as
 * specified by the environment variable SPMASK are not what we need.
 *
 * Returns 1 on success, 0 on failure (which only happens when too many
 * columns are specified).
 */
{
	int i;

	if (ncols > NCOLUMNS) {
		/* doesn't fit in _ds_colmasks[] */
        	return 0;
	}
	for (i = 0; i < ncols; i++) {
        	_ds_colmasks[i] = columns[i];
	}
	_ds_ncolumns = ncols;
	return 1;
}
