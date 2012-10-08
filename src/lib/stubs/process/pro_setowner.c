/*	@(#)pro_setowner.c	1.3	96/02/27 11:16:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pro_setowner
**	Set the owner of a process to be someone else.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"
#include "module/buffers.h"

#define	BFSZ	(2 * sizeof (capability))

errstat
pro_setowner(process, newowner)
capability *	process;	/* in: capability of process to change owner */
capability *	newowner;	/* out: capability of the new owner */
{
    header	h;
    bufsize	err;
    char *	p;
    char	buf[BFSZ];
    
    h.h_port = process->cap_port;
    h.h_priv = process->cap_priv;
    h.h_command = PS_SETOWNER;

/*
** Insert the new owner's capability in the buffer.
** Something is terribly wrong if this fails.
** Probably newowner is a null pointer!
*/
    if ((p = buf_put_cap(buf, buf+BFSZ, newowner)) == 0)
	return STD_ARGBAD;

    if ((err = trans(&h, buf, (bufsize)(p - buf), &h, NILBUF, 0)) != 0)
	return (short) err; /*AMOEBA4*/

    return (short) h.h_status; /*AMOEBA4*/
}
