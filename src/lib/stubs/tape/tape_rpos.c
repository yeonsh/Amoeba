/*	@(#)tape_rpos.c	1.3	96/02/27 11:20:09 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_rpos.c
**	This file contains the library stubs for the tape server TAPE_REC_POS
**	command.
**
** Peter Bosch, 200890, peterb@cwi.nl
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/tape.h"

/*
** tape_rpos, return the record position.
**
*/
errstat
tape_rpos(cap, pos)
	capability *cap;
	int32      *pos;
{
	header  hdr;
	bufsize n;

	hdr.h_port    = cap->cap_port;
	hdr.h_priv    = cap->cap_priv;
	hdr.h_command = TAPE_REC_POS;
	*pos          = 0;
	n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
	if (ERR_STATUS(n)) return(ERR_CONVERT(n));
	if (ERR_STATUS(hdr.h_status)) return(ERR_CONVERT(hdr.h_status));

	*pos = hdr.h_offset;
	return STD_OK;
}
