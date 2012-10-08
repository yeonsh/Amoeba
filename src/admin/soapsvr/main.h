/*	@(#)main.h	1.3	96/02/27 10:22:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef MAIN_H
#define MAIN_H

/*
 * Routines to (try to) lock and unlock global mutex
 */
int  sp_try	_ARGS((void));
int  for_me	_ARGS((header *hdr));

/*
 * fully_initialised() tells whether we have an up to date super file,
 * and initialised sp_table.
 */
int fully_initialised _ARGS((void));

#include "diag.h"

errstat to_other_side  _ARGS((int ntries,
			      header *inhdr, char *inbuf,  bufsize insize,
			      header *outhdr, char *outbuf, bufsize outsize,
			      bufsize *retsize));


#endif
