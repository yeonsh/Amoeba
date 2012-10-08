/*	@(#)ps_segcreate.c	1.3	96/02/27 11:17:23 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** ps_segcreate - create a memory segment
**	Creates a memory segment on the machine specified by 'host'.  If
**	'clone' is not the null-pointer then the segment created is a copy
**	of the segment specified by 'clone'.  ('clone' may well be a bullet
**	file capability.)
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"
#include "module/buffers.h"

#define	BFSZ	(2 * sizeof (capability))

errstat
ps_segcreate(host, size, clone, segcap)
capability *	host;	/* in: capability of processor */
long		size;	/* in: size of segment */
capability *	clone;	/* in: capability of segment to clone */
capability *	segcap;	/* out: capability for segment created */
{
    header	h;
    bufsize	err;
    char *	p;
    char	buf[BFSZ];
    
    h.h_port = host->cap_port;
    h.h_priv = host->cap_priv;
    h.h_command = PS_SEG_CREATE;
    h.h_offset = size;

/*
** Insert the clone's capability in the buffer.
** Something is terribly wrong if this fails.
*/
    if (clone != 0)
    {
	if ((p = buf_put_cap(buf, buf + BFSZ, clone)) == 0)
	    return STD_ARGBAD;
    }
    else
	p = buf;

    if ((err = trans(&h, buf, (bufsize)(p - buf), &h, NILBUF, 0)) != 0)
	return (short) err; /*AMOEBA4*/

    segcap->cap_port = h.h_port;
    segcap->cap_priv = h.h_priv;
    return (short) h.h_status; /*AMOEBA4*/
}
