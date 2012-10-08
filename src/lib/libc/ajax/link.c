/*	@(#)link.c	1.3	94/04/07 10:28:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* link(2) system call emulation */

#include "ajax.h"

int
link(file1, file2)
	const char *file1;
	const char *file2;
{
	long cols[SP_MAXCOLUMNS];
	int ncols;
	capset cs;
	errstat err;

	if ((err = sp_lookup(SP_DEFAULT, file1, &cs)) != STD_OK)
		ERR(ENOENT, "link: file1 not found");
	ncols = sp_mask(SP_MAXCOLUMNS, cols); /* size of cols array */
	err = sp_append(SP_DEFAULT, file2, &cs, ncols, cols);
	cs_free(&cs);
	if (err != STD_OK)
		ERR(EEXIST, "link: append failed");
	return 0;
}
