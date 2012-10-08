/*	@(#)ps_segwrite.c	1.3	96/02/27 11:17:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** ps_segwrite - write data into a memory segment
**	Writes 'size' bytes of data from 'buf' in the memory segment specified
**	by 'segcap', beginning 'offset' bytes from the start of the segment.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"


errstat
ps_segwrite(segcap, offset, buf, size)
capability *	segcap;	/* in: capability of segment to write */
long		offset; /* in: offset within the segment to begin the write */
char *		buf;	/* in: data to write into segment */
long		size;	/* in: #bytes of data to write from buf */
{
    header	h;
    bufsize	err;
    
    while (size > 0)
    {
	h.h_port = segcap->cap_port;
	h.h_priv = segcap->cap_priv;
	h.h_command = PS_SEG_WRITE;
	h.h_offset = offset;
	h.h_size = size > PSVR_BUFSZ ? PSVR_BUFSZ : size;

	if ((err = trans(&h, buf, h.h_size, &h, NILBUF, 0)) != 0)
	    return (short) err; /*AMOEBA4*/
	if (h.h_status != STD_OK)
	    return (short) h.h_status; /*AMOEBA4*/

	buf += h.h_size;
	offset += h.h_size;
	size -= h.h_size;
    }
    return STD_OK;
}
