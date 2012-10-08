/*	@(#)sp_discard.c	1.2	94/04/07 11:09:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

errstat
sp_discard(dir)
capset *dir;
{
    return sp_mktrans(1, dir, (header *) NULL, SP_DISCARD,
		      NILBUF, (bufsize) 0, NILBUF, (bufsize) 0);
}
