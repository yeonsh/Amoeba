/*	@(#)cs_free.c	1.2	94/04/07 09:58:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "capset.h"
#include "stdlib.h"

void cs_free(cs)
capset *cs;
{

    if (cs->cs_suite != 0)
    {
	free((char *) cs->cs_suite);
	cs->cs_suite = 0;
    }
    cs->cs_initial = cs->cs_final = 0;
}
