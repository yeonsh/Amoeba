/*	@(#)tape_status.c	1.3	96/02/27 11:20:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_status.c
**	This file contains the library stubs for the tape server STD_STATUS
**	command.
**
** Peter Bosch, 181289, peterb@cwi.nl
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "module/tape.h"

/*
** tape_status, return the status of a unit...
**
*/
#ifdef __STDC__
errstat
tape_status
(
	capability *cap,
	bufptr	    buf,
	bufsize	    size
)
#else
errstat
tape_status(cap, buf, size)
	capability *cap;
	bufptr	    buf;
	bufsize	    size;
#endif
{
	header  hdr;
	bufsize n;

	hdr.h_port    = cap->cap_port;
	hdr.h_priv    = cap->cap_priv;
	hdr.h_command = STD_STATUS;
	n = trans(&hdr, NILBUF, 0, &hdr, buf, size);
	return(ERR_STATUS(n)? ERR_CONVERT(n): ERR_CONVERT(hdr.h_status));
}
