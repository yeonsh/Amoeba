/*	@(#)pro_setcomm.c	1.3	96/02/27 11:16:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pro_setcomment
**	set the comment string (usually the command line) of a proc
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"
#include "string.h"

errstat
pro_setcomment(process, str)
capability *	process;    /* in: capability of process to set comment of */
char *		str;	    /* in: the string */
{
    header	h;
    bufsize	n;
    
    h.h_port = process->cap_port;
    h.h_priv = process->cap_priv;
    h.h_command = PS_SETCOMMENT;
    n = trans(&h, str, (bufsize) strlen(str), &h, NILBUF, 0);
    if (ERR_STATUS(n))
	return ERR_CONVERT(n);
    return ERR_CONVERT(h.h_status);
}
