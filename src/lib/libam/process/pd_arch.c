/*	@(#)pd_arch.c	1.5	96/02/27 11:03:24 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <string.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "module/proc.h"

/*
 * Function to extract the architecture identifier from a process descriptor.
 */
char *
pd_arch(pd)
process_d *pd;
{
    return pd->pd_magic;
}

