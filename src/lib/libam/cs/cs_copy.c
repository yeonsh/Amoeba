/*	@(#)cs_copy.c	1.2	94/04/07 09:58:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "capset.h"
#include "stdlib.h"

cs_copy(new, cs)
capset *new, *cs;
{

    register i;

    new->cs_initial = cs->cs_initial;
    if ((new->cs_final = cs->cs_final) == 0) {
	new->cs_suite = (suite *)0;
	return 1;
    }

    if ((new->cs_suite = (suite *) calloc(sizeof(suite),
    					(unsigned int) cs->cs_final)) == 0)
	return 0;

    for (i = 0; i < cs->cs_final; i++)
	new->cs_suite[i] = cs->cs_suite[i];
    return 1;
}
