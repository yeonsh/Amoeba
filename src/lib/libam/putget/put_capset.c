/*	@(#)put_capset.c	1.4	96/02/27 11:04:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "capset.h"
#include "module/buffers.h"

char *
buf_put_capset(p, e, cs)
char *		p;
char *		e;
capset *	cs;
{
    register int i;

    if (cs->cs_final < 0 || cs->cs_initial < 0 ||
        cs->cs_initial > cs->cs_final || cs->cs_final > MAXCAPSET)
    {
	return 0;
    }

    p = buf_put_int16(p, e, (int16) cs->cs_initial);
    p = buf_put_int16(p, e, (int16) cs->cs_final);
    for (i = 0; i < cs->cs_final; i++)
    {
	    p = buf_put_cap(p, e, &cs->cs_suite[i].s_object);
	    p = buf_put_int16(p, e, (int16) cs->cs_suite[i].s_current);
    }
    return p;
}
