/*	@(#)sys_printbuf.c	1.3	96/02/27 11:19:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "systask/systask.h"
#include "module/systask.h"


#ifdef __STDC__
errstat
sys_printbuf
(
    capability *	cap,	     /* in:  capability for kernel's systask */
    char *		buf,	     /* in:  ptr to buffer to receive data */
    bufsize		bufsz,	     /* in:  size of 'buf' */
    int *		offset,	     /* out: start of buffer */
    int *		num_bytes    /* out: # bytes returned in 'buf' */
)
#else
errstat
sys_printbuf(cap, buf, bufsz, offset, num_bytes)
capability *	cap;		/* in:  capability for kernel's systask */
char *		buf;		/* in:  ptr to buffer to receive data */
bufsize		bufsz;		/* in:  size of 'buf' */
int *		offset;		/* out: start of buffer */
int *		num_bytes;	/* out: # bytes returned in 'buf' */
#endif
{
    header	hdr;
    bufsize	t;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = SYS_PRINTBUF;

    t = trans(&hdr, NILBUF, 0, &hdr, buf, bufsz);
    if (ERR_STATUS(t))
	return ERR_CONVERT(t);

    *num_bytes = (int) t;
    *offset = (int) hdr.h_offset;
    return ERR_CONVERT(hdr.h_status);
}
