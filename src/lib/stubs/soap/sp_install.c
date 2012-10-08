/*	@(#)sp_install.c	1.2	94/04/07 11:09:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"
#include "sp_buf.h"

static capability NULLCAP;

errstat
sp_install(n, entries, capsets, oldcaps)
int         n;
sp_entry    entries[];
capset      capsets[];
capability *oldcaps[];
{
    union sp_buf *inbuf = NULL;
    int     i;
    char   *p, *e;
    errstat err;

    if (n <= 0) { /* sanity check */
	return STD_ARGBAD;
    }

    if ((inbuf = sp_getbuf()) == NULL) {
	return STD_NOSPACE;
    }
    
    p = inbuf->sp_buffer;
    e = &inbuf->sp_buffer[sizeof(inbuf->sp_buffer)];

    for (i = 0; i < n; i++) {
	p = buf_put_capset(p, e, &entries[i].se_capset);
	p = buf_put_string(p, e, entries[i].se_name);
	p = buf_put_capset(p, e, &capsets[i]);
	p = buf_put_cap(p, e, oldcaps[i] ? oldcaps[i] : &NULLCAP);
    }

    if (p == NULL) {
	err = STD_NOSPACE;
    } else {
	err = sp_mktrans(1, &entries[0].se_capset, (header *) NULL, SP_INSTALL,
			 inbuf->sp_buffer, (bufsize) (p - inbuf->sp_buffer),
			 NILBUF, (bufsize) 0);
    }

    sp_putbuf(inbuf);
    return err;
}
