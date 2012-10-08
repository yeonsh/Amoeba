/*	@(#)pro_stun.c	1.3	96/02/27 11:16:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pro_stun - Stop a process (by signalling it).
**	The call returns successfully as soon as the kernel has determined
**	that the process exists and can be stunned.
**	When the process has actually come to rest, its owner receives a
**	checkpoint with cause PS_STUNNED and detail as the second parameter.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "module/proc.h"

errstat
pro_stun(process, detail)
capability *	process;	/* in: capability of process to stun */
long		detail;		/* in: parameter to use in checkpoint */
{
    header	h;
    bufsize	err;
    
    h.h_port = process->cap_port;
    h.h_priv = process->cap_priv;
    h.h_offset = detail;
    h.h_command = PS_STUN;

    if ((err = trans(&h, NILBUF, 0, &h, NILBUF, 0)) != 0)
	return (short) err; /*AMOEBA4*/

    return (short) h.h_status; /*AMOEBA4*/
}
