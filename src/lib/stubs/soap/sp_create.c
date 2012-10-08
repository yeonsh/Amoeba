/*	@(#)sp_create.c	1.2	94/04/07 11:09:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

#define INBUF_SIZE	(SP_MAXCOLUMNS * (NAME_MAX + 1))

errstat
sp_create(server, columns, dir)
capset *server;
char   *columns[];
capset *dir;
{
    char    inbuf[INBUF_SIZE];
    char   *p, *e;
    capset  svr;
    errstat err;

    /* Get capset for server on which to create new directory: */
    if (server == SP_DEFAULT) {
	err = sp_get_workdir(&svr);
    } else {
	err = cs_copy(&svr, server) ? STD_OK : STD_NOSPACE;
    }
    if (err != STD_OK) {
	return err;
    }

    p = inbuf;
    e = inbuf + sizeof(inbuf);
    while (*columns != NULL) {
	p = buf_put_string(p, e, *columns);
	columns++;
    }

    if (p == NULL) {
	err = STD_NOSPACE;
    } else {
	header hdr;

	err = sp_mktrans(SP_NTRY, &svr, &hdr, SP_CREATE,
			 inbuf, (bufsize) (p - inbuf), NILBUF, (bufsize) 0);

	if (err == STD_OK) {
	    /* turn capability for new directory into capset */
	    capability cap;

	    cap.cap_port = hdr.h_port;
	    cap.cap_priv = hdr.h_priv;
	    err = cs_singleton(dir, &cap) ? STD_OK : STD_NOSPACE;
	}
    }

    cs_free(&svr);
    return err;
}
