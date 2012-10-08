/*	@(#)pro_getload.c	1.3	96/02/27 11:16:32 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pro_getload - Get processor load
**	Asks the process/segment server specified by 'cap' for its current
**	load average, free memory and meaningless indication of processor
**	speed (multiplied by 1024).
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"

errstat
pro_getload(cap, ips, loadav, mfree)
capability *	cap;	/* in:  process/segment server capability */
long *		ips;	/* out: CPU speed, in instructions per second */
long *		loadav;	/* out: average runnable threads, times 1024 */
long *		mfree;	/* out: free memory, in bytes */
{
    header	hdr;
    bufsize	reply;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = PS_GETLOAD;

    reply = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
    if (ERR_STATUS(reply))
	return ERR_CONVERT(reply);

    if (hdr.h_status == STD_OK)
    {
	*ips = (long) hdr.h_extra * 1024;
	*loadav = (long) hdr.h_size;
	*mfree  = hdr.h_offset;
    }

    return ERR_CONVERT(hdr.h_status);
}
