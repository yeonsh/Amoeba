/*	@(#)sp_getmasks.c	1.3	94/04/07 11:09:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

#define OUTBUF_SIZE	(SP_MAXCOLUMNS * sizeof(long))

errstat
sp_getmasks(dir, name, ncols, cols)
capset     *dir;
const char *name;
int         ncols;
rights_bits cols[];
{
    char    outbuf[OUTBUF_SIZE];
    header  hdr;
    capset  last;
    errstat err;

    /* sanity check on the number of columns asked for: */
    if (ncols < 1 || ncols > SP_MAXCOLUMNS) {
	return STD_ARGBAD;
    }

    if ((err = sp_traverse(dir, &name, &last)) != STD_OK) {
	return err;
    }

    err = sp_mktrans(SP_NTRY, &last, &hdr, SP_GETMASKS,
		     name, (bufsize) (strlen(name) + 1),
		     outbuf, (bufsize) sizeof(outbuf));

    if (err == STD_OK) {
	/* Fetch masks from outbuf: */
	char *p, *e;
	int   col;

	p = outbuf;
	e = &outbuf[hdr.h_size];
	for (col = 0; col < ncols; col++) {
	    if ((p = buf_get_right_bits(p, e, &cols[col])) == NULL) {
		/* the directory has less columns than expected */
		break;
	    }
	}
	/* Zero the remaining columns.
	 * TODO: change the interface so that we can also return the number
	 * of column masks that *were* present.
	 */
	for ( ; col < ncols; col++) {
	    cols[col] = 0;
	}
    }
    
    cs_free(&last);
    return err;
}
