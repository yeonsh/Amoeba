/*	@(#)iop_setcmap.c	1.2	94/04/07 11:03:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * For now this is sun specific.  When we know how other systems do it
 * we can make this more portable
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdlib.h"
#include "string.h"
#include "iop/iop.h"
#include "iop/colourmap.h"
#include "module/buffers.h"

errstat
iop_setcmap(svr, cmapp)
capability *	svr;
colour_map *	cmapp;
{
    header	hdr;
    bufsize	err;
    char *	buf;
    char *	end;
    char *	p;
    size_t	size;

    /* Sanity check - may not be valid */
    if (cmapp->c_count < 0 || cmapp->c_count > 1024)
	return STD_ARGBAD;
    size = cmapp->c_count * 3 + 2 * sizeof (int);
    buf = (char *) malloc(size);
    if (buf == 0)
	return STD_NOMEM;
    end = buf + size;
    p = buf_put_long(buf, end, cmapp->c_index);
    p = buf_put_long(p, end, cmapp->c_count);
    if (p == 0) /* This can't happen in a sane world */
	return STD_SYSERR;
    (void) memmove((_VOIDSTAR) p, (_VOIDSTAR) cmapp->c_red, cmapp->c_count);
    p += cmapp->c_count;
    (void) memmove((_VOIDSTAR) p, (_VOIDSTAR) cmapp->c_green, cmapp->c_count);
    p += cmapp->c_count;
    (void) memmove((_VOIDSTAR) p, (_VOIDSTAR) cmapp->c_blue, cmapp->c_count);
    /* Another sanity check */
    p += cmapp->c_count;
    if (p > end)
	return STD_SYSERR;

    hdr.h_port = svr->cap_port;
    hdr.h_priv = svr->cap_priv;
    hdr.h_command = IOP_SETCMAP;
    err = trans(&hdr, (bufptr) buf, (bufsize) size, &hdr, NILBUF, 0);
    free((_VOIDSTAR) buf);
    if (ERR_STATUS(err))
	return ERR_CONVERT(err);
    return ERR_CONVERT(hdr.h_status);
}
