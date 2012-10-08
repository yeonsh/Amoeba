/*	@(#)pro_getdef.c	1.3	96/02/27 11:16:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**  pro_getdef - Get definitions
**	Get MMU parameters for machine specified by 'host' from process/segment
**	server.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"

errstat
pro_getdef(host, size, first, last)
capability *	host;	/* in: Capability for segment server */
int *		size;	/* out: log2(pagesize) */
int *		first;	/* out: First user page */
int *		last;	/* out: Last user page + 1 */
{
    header	hdr;
    bufsize	len;

    hdr.h_port = host->cap_port;
    hdr.h_priv = host->cap_priv;
    hdr.h_command = PS_GETDEF;

    if ((len = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return (short) len; /*AMOEBA4*/

    if (hdr.h_status == STD_OK)
    {
	*first = hdr.h_extra;
	*last = hdr.h_offset;
	*size = hdr.h_size;
    }
    return (short) hdr.h_status; /*AMOEBA4*/
}
