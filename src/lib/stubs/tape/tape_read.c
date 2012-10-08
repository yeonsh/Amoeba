/*	@(#)tape_read.c	1.5	96/02/29 16:48:46 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_read.c
**	This file contains the library stubs for the tape server TAPE_READ
**	command.
**
** Peter Bosch, 181289, peterb@cwi.nl
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/tape.h"

/*
** Read record_size bytes into buf...
**
*/
#ifdef __STDC__
errstat
tape_read
(
	capability *	cap,		/* in:	capability for tape server */
	bufptr		buf,		/* in:  place to read bytes into */
	bufsize		record_size,	/* in:  #bytes to be read */
	bufsize *	xread		/* out: # bytes actually read */
)
#else
errstat
tape_read(cap, buf, record_size, xread)
	capability *	cap;		/* in:	capability for tape server */
	bufptr		buf;		/* in:  place to read bytes into */
	bufsize		record_size;	/* in:  #bytes to be read */
	bufsize *	xread;		/* out: # bytes actually read */
#endif
{
	header	hdr;
	bufsize	n;

	if (record_size > (bufsize) T_REQBUFSZ)
		return(STD_ARGBAD);

	*xread 	      = 0;
	hdr.h_port    = cap->cap_port;
	hdr.h_priv    = cap->cap_priv;
	hdr.h_command = TAPE_READ;
	hdr.h_size    = record_size;
	n = trans(&hdr, NILBUF, 0, &hdr, buf, record_size);
	if (ERR_STATUS(n))
		return(ERR_CONVERT(n));
	/* We have to treat EOT as a warning not an error ... */
        *xread        = hdr.h_size;
	if (hdr.h_status != STD_OK)
		return(ERR_CONVERT(hdr.h_status));
	return(STD_OK);
}
