/*	@(#)fswrite.c	1.4	96/02/27 11:15:02 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "file.h"

long
fswrite(cap, position, buf, size)
capability *cap;
long position;
char *buf;
long size;
{
    header hdr;
    bufsize n;
    register long total = 0;

    if (size < 0) {
	return STD_ARGBAD;
    }
    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_size = BUFFERSIZE;
    do {
	hdr.h_command = FSQ_WRITE;
	hdr.h_offset = position;
	if (size < (long) hdr.h_size)
	    hdr.h_size = size;
	if ((n = trans(&hdr, buf, hdr.h_size, &hdr, NILBUF, 0)) != 0)
	    return ERR_CONVERT(n);
	if (hdr.h_status != STD_OK)
	    return ERR_CONVERT(hdr.h_status);
	total	 += hdr.h_size;
	buf	 += hdr.h_size;
	size	 -= hdr.h_size;
	position += hdr.h_size;
    } while (size != 0 && hdr.h_size != 0);
    return total;
}
