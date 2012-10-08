/*	@(#)tape_fskip.c	1.3	96/02/27 11:19:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_fskip.c
**	This file contains the library stubs for the tape server TAPE_FILE_SKIP
**	command.
**
** Peter Bosch, 181289, peterb@cwi.nl
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/tape.h"

/*
** tape_fskip, skip a number of files...
**
*/
errstat
tape_fskip(cap, count)
	capability *cap;
	int32       count;
{
	header  hdr;
	bufsize n;

	hdr.h_port    = cap->cap_port;
	hdr.h_priv    = cap->cap_priv;
	hdr.h_command = TAPE_FILE_SKIP;
	hdr.h_offset  = count;
	n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
	return(ERR_STATUS(n)? ERR_CONVERT(n): ERR_CONVERT(hdr.h_status));
}
