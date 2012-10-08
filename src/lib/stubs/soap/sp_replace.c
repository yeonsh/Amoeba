/*	@(#)sp_replace.c	1.3	94/04/07 11:09:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

#define INBUF_SIZE	((PATH_MAX + 1) + CAPSETBUFSIZE)

errstat
sp_replace(dir, name, cs)
capset     *dir;
const char *name;
capset     *cs;
{
    char    inbuf[INBUF_SIZE];
    char   *p, *e;
    capset  last;
    errstat err;

    if ((err = sp_traverse(dir, &name, &last)) != STD_OK) {
	return err;
    }

    p = inbuf;
    e = inbuf + sizeof(inbuf);
    p = buf_put_string(inbuf, e, name);
    p = buf_put_capset(p, e, cs);

    if (p == NULL) {
	err = STD_NOSPACE;
    } else {
	err = sp_mktrans(1, &last, (header *) NULL, SP_REPLACE,
			 inbuf, (bufsize) (p - inbuf), NILBUF, (bufsize) 0);
    }

    cs_free(&last);
    return err;
}
