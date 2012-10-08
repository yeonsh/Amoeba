/*	@(#)fsread.c	1.4	96/02/27 11:14:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "file.h"

/*
 * Some history:
 * Hdr.h_extra used to be an input field telling to do non-blocking read
 * when set. This was an ugly hack needed to convince people that terminals
 * were just like files, which they are not.
 * Now it is an output field telling the read was short because the data
 * ran out, instead of being short because the other guy didn't have big enough
 * a buffer.
 *
 * We need to talk about this some day.
 */
#define	MAX_READ	30000

long
fsread(cap, position, buf, size)
capability *cap;
long position;
char *buf;
long size;
{
    header hdr;
    register uint16 n;
    register long total = 0;
    bufsize t;

    if (size < 0) {
	return STD_ARGBAD;
    }
    do {
	hdr.h_port = cap->cap_port;
	hdr.h_priv = cap->cap_priv;
	hdr.h_extra = 0;		/* backward compatibility */
	hdr.h_command = FSQ_READ;
	hdr.h_offset = position;
	hdr.h_size = n = size > MAX_READ ? MAX_READ : size;
	t = trans(&hdr, NILBUF, 0, &hdr, buf, n);
	if (ERR_STATUS(t))
	    return ERR_CONVERT(t);
	if (hdr.h_status != STD_OK)
	    return ERR_CONVERT(hdr.h_status);
	total	 += hdr.h_size;
	buf	 += hdr.h_size;
	size	 -= hdr.h_size;
	position += hdr.h_size;
    } while (size != 0 && hdr.h_extra == FSE_MOREDATA && hdr.h_size != 0);
    return total;
}
