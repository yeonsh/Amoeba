/*	@(#)tod_defcap.c	1.3	96/02/27 11:20:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "ampolicy.h"
#include "tod/tod.h"
#include "module/tod.h"
#include "module/name.h"

errstat
tod_defcap()
{
	static capability cap;
	capability *tod;
	errstat err;

	if ((tod = getcap("TOD")) == 0)
	{
		if ((err = name_lookup(DEF_TODSVR, &cap)) == STD_OK)
			tod_setcap(&cap);
		return err;
	}
	tod_setcap(tod);
	return STD_OK;
}
