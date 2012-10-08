/*	@(#)ajmodemt.c	1.2	94/04/07 10:23:23 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Return mode and mtime from a type capability (or default) */
/* Obsolete -- this dummy lasts until I've removed all uses */

#include "ajax.h"

int
_ajax_modemtime(ptypecap, pmode, pmtime)
	capability *ptypecap;
	int *pmode;
	long *pmtime;
{
	if (pmode != NULL)
		*pmode = 0777; /* Generous mode */
	if (pmtime != NULL)
		*pmtime = 0; /* Epoch as mtime */
	return 1; /* Nonzero (not quite OK), but positive (no error) */
}
