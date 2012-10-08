/*	@(#)sp_getseqno.c	1.1	96/02/27 11:17:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

#define OUTBUF_SIZE	(sizeof(sp_seqno_t))

errstat
sp_getseqno(dir, seqno)
capset     *dir;
sp_seqno_t *seqno;
{
    char    outbuf[OUTBUF_SIZE];
    header  hdr;
    errstat err;

    err = sp_mktrans(SP_NTRY, dir, &hdr, SP_GETSEQNR,
		     (char *) 0, (bufsize) 0,
		     outbuf, (bufsize) sizeof(outbuf));

    if (err == STD_OK) {
	/* Fetch seqno from outbuf: */
	char *p, *e;

	p = outbuf;
	e = &outbuf[hdr.h_size];

	p = buf_get_uint32(p, e, &seqno->seq[0]);
	p = buf_get_uint32(p, e, &seqno->seq[1]);
	if (p == NULL) {
	    err = STD_SYSERR;
	}
    }
    
    return err;
}
