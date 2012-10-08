/*	@(#)tod_setcap.c	1.3	96/02/27 11:21:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/tod.h"

extern capability *tod_cap;

void
tod_setcap(cap)
capability *cap;
{
    tod_cap = cap;
}
