/*	@(#)pro_getcomm.c	1.3	96/02/27 11:16:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** pro_getcomment
**	Find the comment string (usually the command line) of a proc
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"

errstat
pro_getcomment(process, buf, len)
capability *	process;    /* in:  capability of process to find comment of */
char *		buf;	    /* out: where to put the string */
int		len;	    /* in:  how big the string may be */
{
    header	h;
    bufsize	n;
    
    h.h_port = process->cap_port;
    h.h_priv = process->cap_priv;
    h.h_command = PS_GETCOMMENT;
    n = trans(&h, NILBUF, 0, &h, buf, (bufsize) (len-1));
    if (ERR_STATUS(n))
	return ERR_CONVERT(n);
    buf[n] = '\0';
    return ERR_CONVERT(h.h_status);
}
