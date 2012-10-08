/*	@(#)sp_lookup.c	1.5	94/04/07 11:09:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

/* Protection against infinitely looping: */
#define MAX_STEPS	10
#define INBUF_SIZE	(PATH_MAX + 1)
#define OUTBUF_SIZE	(PATH_MAX + 1 + CAPSETBUFSIZE)

errstat
sp_lookup(dir, path, cs)
capset     *dir;
const char *path;
capset     *cs;
{
    char    inbuf[INBUF_SIZE];   /* pathname for SP_LOOKUP request */
    char    outbuf[OUTBUF_SIZE]; /* SP_LOOKUP reply & initial path_cs_norm */
    header  hdr;
    int	    nsteps;
    char   *p, *e;
    errstat err;

#ifndef KERNEL
    /* Evaluate any "." or ".." components in name: */
    if (path_cs_norm(&dir, path, outbuf, sizeof outbuf) >= 0) {
	path = outbuf;
    }
#endif
    
    if (dir == SP_DEFAULT) {
	if (*path == '/') {
	    err = sp_get_rootdir(cs);
	} else {
	    err = sp_get_workdir(cs);
	}
    } else {
	err = cs_copy(cs, dir) ? STD_OK : STD_NOSPACE;
    }
    if (err != STD_OK) {
	return err;
    }

    nsteps = 0;
    while (err == STD_OK) {
	while (*path == '/') {
	    path++;
	}
	if (*path == '\0' || (path[0] == '.' && path[1] == '\0')) {
	    return STD_OK;
	}

	if (nsteps++ > MAX_STEPS) {
	    err = STD_OVERFLOW;
	    break;
	}

	p = buf_put_string(inbuf, &inbuf[sizeof(inbuf)], path);
	if (p == NULL) {
	    err = STD_NOSPACE;
	    break;
	}

	err = sp_mktrans(SP_NTRY, cs, &hdr, SP_LOOKUP,
			 inbuf, (bufsize) (p - inbuf),
			 outbuf, (bufsize) sizeof(outbuf));

	if (err == STD_OK) {
	    e = &outbuf[hdr.h_size];
	    p = buf_get_string(outbuf, e, (char **) &path);

	    cs_free(cs);
	    p = buf_get_capset(p, e, cs);
	    if (p == NULL) {
		err = STD_NOSPACE;
	    }
	}
    }

    cs_free(cs);
    return err;
}
