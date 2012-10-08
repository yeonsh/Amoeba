/*	@(#)pro_getowner.c	1.3	96/02/27 11:16:37 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pro_getowner
**	Find the capability of the owner of a particular process.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"

errstat
pro_getowner(process, cur_owner)
capability *	process;    /* in:  capability of process to find owner of */
capability *	cur_owner;  /* out: current process owner */
{
    header	h;
    bufsize	err;
    
    h.h_port = process->cap_port;
    h.h_priv = process->cap_priv;
    h.h_command = PS_GETOWNER;

    if ((err = trans(&h, NILBUF, 0, &h, NILBUF, 0)) != 0)
	return (short) err; /*AMOEBA4*/

    if (h.h_status == STD_OK)
    {
	cur_owner->cap_port = h.h_port;
	cur_owner->cap_priv = h.h_priv;
    }

    return (short) h.h_status; /*AMOEBA4*/
}
