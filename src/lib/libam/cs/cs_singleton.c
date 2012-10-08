/*	@(#)cs_singleton.c	1.2	94/04/07 09:59:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "capset.h"
#include "stdlib.h"

cs_singleton(cs, cap)
capset *cs;
capability *cap;
{
    suite *s;

    if ((s = (suite *) malloc(sizeof(suite))) == 0)
	return 0;
    s->s_current = 1;
    s->s_object = *cap;
    cs->cs_initial = cs->cs_final = 1;
    cs->cs_suite = s;
    return 1;
}
