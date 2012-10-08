/*	@(#)pro_exec.c	1.4	96/02/27 11:16:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pro_exec - Execute a process.
**	This is a very low-level interface.  It gives a process descriptor to
**	the process/segment server on the machine specified by 'host' which
**	starts the process if all is well.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdlib.h"
#include "module/proc.h"

errstat
pro_exec(host, pd, ret_process)
capability *	host;		/* in: capability for segsvr on CPU to use */
process_d *	pd;		/* in: process descriptor of process to start */
capability *	ret_process;	/* out: capability of process created */
{
    char *		buf;
    header		hdr;
    int			size;
    char *		p;
    bufsize		reply;

/* find out how big the process descriptor is */
    if ((size = pd_size(pd)) <= 0)
	return STD_ARGBAD;

/* allocate a buffer that should be big enough for the pd */
    if ((buf = (char *) malloc((size_t) size)) == 0)
	return STD_NOMEM;

/* pack the process descriptor into a buffer */
    if ((p = buf_put_pd(buf, buf + size, pd)) == 0)
    {
	free(buf);
	return STD_ARGBAD;
    }

/* send the command to the process server */
    hdr.h_port = host->cap_port;
    hdr.h_priv = host->cap_priv;
    hdr.h_size = (bufsize) size;
    hdr.h_command = PS_EXEC;
    
    reply = trans(&hdr, buf, (bufsize)(p - buf), &hdr, NILBUF, 0);
    free(buf);

    if (reply != 0)
	return (short) reply; /*AMOEBA4*/

    if (hdr.h_status == STD_OK)
    {
	ret_process->cap_port = hdr.h_port;
	ret_process->cap_priv = hdr.h_priv;
    }

    return (short) hdr.h_status; /*AMOEBA4*/
}
