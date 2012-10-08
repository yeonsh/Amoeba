/*	@(#)sp_chmod.c	1.3	94/04/07 11:08:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

#define INBUF_SIZE	((PATH_MAX + 1) + (SP_MAXCOLUMNS * sizeof(long)))

errstat
sp_chmod(dir, name, ncols, cols)
capset     *dir;
const char *name;
int	    ncols;
rights_bits cols[];
{
    char    inbuf[INBUF_SIZE];
    char   *p, *e;
    capset  last;
    int     n;
    errstat err;

    if ((err = sp_traverse(dir, &name, &last)) != STD_OK) {
	return err;
    }

    p = inbuf;
    e = inbuf + sizeof(inbuf);

    p = buf_put_string(p, e, name);
    for (n = 0; n < ncols; n++) {
	p = buf_put_right_bits(p, e, cols[n]);
    }

    if (p == NULL) {
	/* didn't fit because of too many columns or name too long */
	err = STD_ARGBAD;
    } else {
	err = sp_mktrans(SP_NTRY, &last, (header *) NULL, SP_CHMOD,
			 inbuf, (bufsize) (p - inbuf), NILBUF, (bufsize) 0);
    }

    cs_free(&last);
    return err;
}
