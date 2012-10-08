/*	@(#)b_read.c	1.3	94/04/07 11:00:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_read
**	The bullet server reader client stub.
**	This routine returns the error status of the read.
**	The results of the function all come through the parameters.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"

errstat
b_read(cap, offset, buf, size, num_read)
capability *	cap;		/* in: capability of file to be read */
b_fsize		offset;		/* in: first byte to read from file */
char *		buf;		/* out: place where data is read into */
b_fsize		size;		/* in: number of bytes requested to be read */
b_fsize *	num_read;	/* out: number of bytes actually read */
{
    header	hdr;
    b_fsize	total = 0;
    bufsize	t;
    bufsize	n;

    if (size < 0)
	return STD_ARGBAD;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    do
    {
	hdr.h_command = BS_READ;
	hdr.h_offset = offset;
	hdr.h_size = n = size > BS_REQBUFSZ ? BS_REQBUFSZ : size;
	t = trans(&hdr, NILBUF, 0, &hdr, buf, n);
	if (ERR_STATUS(t))
	{
	    hdr.h_command = BS_READ;
	    hdr.h_offset = offset;
	    hdr.h_size = n = size > BS_REQBUFSZ ? BS_REQBUFSZ : size;
	    t = trans(&hdr, NILBUF, 0, &hdr, buf, n); /* try again */
	    if (ERR_STATUS(t))
		return ERR_CONVERT(t);
	}
	if (hdr.h_status != STD_OK)
	    return ERR_CONVERT(hdr.h_status);
	total += hdr.h_size;
	buf += hdr.h_size;
	size -= hdr.h_size;
	offset += hdr.h_size;
    } while ((size > 0) && (n == hdr.h_size));
    *num_read = total;
    return STD_OK;
}
