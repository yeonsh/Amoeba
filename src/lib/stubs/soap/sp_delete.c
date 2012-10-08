/*	@(#)sp_delete.c	1.3	94/04/07 11:09:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

errstat
sp_delete(dir, name)
capset     *dir;
const char *name;
{
    capset  last;
    errstat err;

    if ((err = sp_traverse(dir, &name, &last)) != STD_OK) {
	return err;
    }

    err = sp_mktrans(1, &last, (header *) NULL, SP_DELETE,
		     name, (bufsize) (strlen(name) + 1), NILBUF, (bufsize) 0);

    cs_free(&last);
    return err;
}
