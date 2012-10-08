/*	@(#)cs_purge.c	1.2	94/04/07 09:59:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "module/stdcmd.h"

errstat
cs_purge(cs)
capset *cs;
{
    errstat	err;
    errstat	rtn;
    register	i;
    suite *	s;

    rtn = STD_OK;
    for (i = 0; i < cs->cs_final; i++)
    {
	s = &cs->cs_suite[i];
	if (!s->s_current)
	    continue;
	if ((err = std_destroy(&s->s_object)) == STD_OK)
	    s->s_current = 0;
	else if (rtn == STD_OK)   /* Set rtn to the first error, if any: */
		rtn = err;
    }
    return rtn;
}
