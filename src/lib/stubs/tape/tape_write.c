/*	@(#)tape_write.c	1.5	96/02/29 16:48:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_write.c
**	This file contains the library stubs for the tape server TAPE_WRITE
**	command.
**
** Peter Bosch, 181289, peterb@cwi.nl
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/tape.h"

/*
** Write record_size bytes to tape...
**
*/
#ifdef __STDC__
errstat
tape_write
(
	capability *	cap,		/* in:  capability for tape unit */
	bufptr		buf,		/* in:  buffer with data to write */
	bufsize		record_size,	/* in:  #bytes to be written */
	bufsize *	xwritten	/* out: #bytes actually written */
)
#else
errstat
tape_write(cap, buf, record_size, xwritten)
	capability *	cap;		/* in:  capability for tape unit */
	bufptr		buf;		/* in:  buffer with data to write */
	bufsize		record_size;	/* in:  #bytes to be written */
	bufsize *	xwritten;	/* out: #bytes actually written */
#endif
{
	header	hdr;
	bufsize	n;

	if (record_size > (bufsize) T_REQBUFSZ)
		return(STD_ARGBAD);

	*xwritten     = 0;
	hdr.h_port    = cap->cap_port;
	hdr.h_priv    = cap->cap_priv;
	hdr.h_command = TAPE_WRITE;
	hdr.h_size    = record_size;

	n = trans(&hdr, buf, hdr.h_size, &hdr, NILBUF, 0);
	if (ERR_STATUS(n))
		return(ERR_CONVERT(n));
	/* We set the xwritten now since EOT looks like an error but isn't */
	*xwritten = hdr.h_size;
	if (ERR_STATUS(hdr.h_status))
		return(ERR_CONVERT(hdr.h_status));
	return(STD_OK);
}
