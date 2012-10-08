/*	@(#)sp_dir.c	1.3	94/04/07 10:19:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "sp_dir.h"
#include "stdlib.h"

SP_DIR *sp_opendir(name)
char *name;
{
	capset dir;
	SP_DIR *dd;
	int ret;

	if (sp_lookup(SP_DEFAULT, name, &dir) != STD_OK)
		return 0;
	ret = sp_list(&dir, &dd);
	cs_free(&dir);
	return ret == STD_OK ? dd : 0;
}

struct sp_direct *sp_readdir(dd)
SP_DIR *dd;
{
	if (dd->dd_curpos < 0 || dd->dd_curpos >= dd->dd_nrows)
		return 0;
	return &dd->dd_rows[dd->dd_curpos++];
}

long sp_telldir(dd)
SP_DIR *dd;
{
	return dd->dd_curpos;
}

void sp_seekdir(dd, pos)
SP_DIR *dd;
long pos;
{
	dd->dd_curpos = (int) pos;
}

void sp_rewinddir(dd)
SP_DIR *dd;
{
	dd->dd_curpos = 0;
}

void sp_closedir(dd)
SP_DIR *dd;
{
	register i;

	cs_free(&dd->dd_capset);
	if (dd->dd_colnames != 0) {
		for (i=0; i<dd->dd_ncols; i++) {
			if ( dd->dd_colnames[i] )
				free((_VOIDSTAR) dd->dd_colnames[i]);
		} 
		free((_VOIDSTAR) dd->dd_colnames);
	}
	if (dd->dd_rows != 0) {
		for (i = 0; i < dd->dd_nrows; i++) {
			if (dd->dd_rows[i].d_name != 0)
				free((_VOIDSTAR) dd->dd_rows[i].d_name);
			if (dd->dd_rows[i].d_columns != 0)
				free((_VOIDSTAR) dd->dd_rows[i].d_columns);
		}
		free((_VOIDSTAR) dd->dd_rows);
	}
	free((_VOIDSTAR) dd);
}
