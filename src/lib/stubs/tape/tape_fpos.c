/*	@(#)tape_fpos.c	1.3	96/02/27 11:19:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_fpos.c
**	This file contains the library stubs for the tape server TAPE_FILE_POS
**	command.
**
** Peter Bosch, 200890, peterb@cwi.nl
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/tape.h"

/*
** tape_fpos, return the file position.
**
*/
errstat
tape_fpos(cap, pos)
	capability *cap;
	int32      *pos;
{
	header  hdr;
	bufsize n;

	hdr.h_port    = cap->cap_port;
	hdr.h_priv    = cap->cap_priv;
	hdr.h_command = TAPE_FILE_POS;
	*pos          = 0;
	n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
	if (ERR_STATUS(n)) return(ERR_CONVERT(n));
	if (ERR_STATUS(hdr.h_status)) return(ERR_CONVERT(hdr.h_status));

	*pos = hdr.h_offset;
	return STD_OK;
}
